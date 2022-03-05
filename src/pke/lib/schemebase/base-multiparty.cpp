//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

/*
Description:

This code implements RNS variants of the Cheon-Kim-Kim-Song scheme.

The CKKS scheme is introduced in the following paper:
- Jung Hee Cheon, Andrey Kim, Miran Kim, and Yongsoo Song. Homomorphic
encryption for arithmetic of approximate numbers. Cryptology ePrint Archive,
Report 2016/421, 2016. https://eprint.iacr.org/2016/421.

 Our implementation builds from the designs here:
 - Marcelo Blatt, Alexander Gusev, Yuriy Polyakov, Kurt Rohloff, and Vinod
Vaikuntanathan. Optimized homomorphic encryption solution for secure genomewide
association studies. Cryptology ePrint Archive, Report 2019/223, 2019.
https://eprint.iacr.org/2019/223.
 - Andrey Kim, Antonis Papadimitriou, and Yuriy Polyakov. Approximate
homomorphic encryption with reduced approximation error. Cryptology ePrint
Archive, Report 2020/1118, 2020. https://eprint.iacr.org/2020/
1118.
 */

#ifndef LBCRYPTO_CRYPTO_BASE_MULTIPARTY_C
#define LBCRYPTO_CRYPTO_BASE_MULTIPARTY_C

#include "cryptocontext.h"
#include "schemebase/base-pke.h"
#include "schemebase/base-multiparty.h"
#include "schemebase/rlwe-cryptoparameters.h"

namespace lbcrypto {

// makeSparse is not used by this scheme
template <class Element>
KeyPair<Element> MultipartyBase<Element>::MultipartyKeyGen(
    CryptoContext<Element> cc, const vector<PrivateKey<Element>> &privateKeyVec,
    bool makeSparse) {
  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersRLWE<Element>>(
          cc->GetCryptoParameters());

  KeyPair<Element> keyPair(std::make_shared<PublicKeyImpl<Element>>(cc),
                           std::make_shared<PrivateKeyImpl<Element>>(cc));

  const shared_ptr<ParmType> elementParams = cryptoParams->GetElementParams();
  const auto ns = cryptoParams->GetNoiseScale();

  const DggType &dgg = cryptoParams->GetDiscreteGaussianGenerator();
  DugType dug;
  TugType tug;

  // Private Key Generation

  Element s(elementParams, Format::EVALUATION, true);

  for (auto &pk : privateKeyVec) {
    const Element &si = pk->GetPrivateElement();
    s += si;
  }

  // Public Key Generation
  Element a(dug, elementParams, Format::EVALUATION);
  Element e(dgg, elementParams, Format::EVALUATION);

  Element b = ns * e - a * s;

  keyPair.secretKey->SetPrivateElement(std::move(s));

  keyPair.publicKey->SetPublicElementAtIndex(0, std::move(b));
  keyPair.publicKey->SetPublicElementAtIndex(1, std::move(a));

  return keyPair;
}

template <class Element>
KeyPair<Element> MultipartyBase<Element>::MultipartyKeyGen(
    CryptoContext<Element> cc, const PublicKey<Element> publicKey,
    bool makeSparse, bool fresh) {
  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersRLWE<Element>>(
          cc->GetCryptoParameters());

  KeyPair<Element> keyPair(std::make_shared<PublicKeyImpl<Element>>(cc),
                           std::make_shared<PrivateKeyImpl<Element>>(cc));

  const shared_ptr<ParmType> elementParams = cryptoParams->GetElementParams();
  const auto ns = cryptoParams->GetNoiseScale();

  const DggType &dgg = cryptoParams->GetDiscreteGaussianGenerator();
  DugType dug;
  TugType tug;

  Element s;
  switch (cryptoParams->GetMode()) {
    case RLWE:
      s = Element(dgg, elementParams, Format::EVALUATION);
      break;
    case OPTIMIZED:
      s = Element(tug, elementParams, Format::EVALUATION);
      break;
    case SPARSE:
      s = Element(tug, elementParams, Format::EVALUATION, 64);
      break;
    default:
      break;
  }

  const std::vector<Element> &pk = publicKey->GetPublicElements();

  Element a = pk[1];
  Element e(dgg, elementParams, Format::EVALUATION);

  // When PRE is not used, a joint key is computed
  Element b = fresh ? ns * e - a * s
                    : ns * e - a * s + pk[0];

  keyPair.secretKey->SetPrivateElement(std::move(s));

  keyPair.publicKey->SetPublicElementAtIndex(0, std::move(b));
  keyPair.publicKey->SetPublicElementAtIndex(1, std::move(a));

  return keyPair;
}

template <class Element>
EvalKey<Element> MultipartyBase<Element>::MultiKeySwitchGen(
    const PrivateKey<Element> oldPrivateKey,
    const PrivateKey<Element> newPrivateKey,
    const EvalKey<Element> evalKey) const {
  return oldPrivateKey->GetCryptoContext()->GetScheme()->KeySwitchGen(
      oldPrivateKey, newPrivateKey, evalKey);
}

template <class Element>
shared_ptr<std::map<usint, EvalKey<Element>>>
MultipartyBase<Element>::MultiEvalAutomorphismKeyGen(
    const PrivateKey<Element> privateKey,
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap,
    const std::vector<usint> &indexList) const {
  const Element &s = privateKey->GetPrivateElement();
  usint N = s.GetRingDimension();

  if (indexList.size() > N - 1)
    PALISADE_THROW(math_error, "size exceeds the ring dimension");

  const auto cc = privateKey->GetCryptoContext();

  auto result = std::make_shared<std::map<usint, EvalKey<Element>>>();

#pragma omp parallel for if (indexList.size() >= 4)
  for (usint i = 0; i < indexList.size(); i++) {
    PrivateKey<Element> privateKeyPermuted =
        std::make_shared<PrivateKeyImpl<Element>>(cc);

    usint index = NativeInteger(indexList[i]).ModInverse(2 * N).ConvertToInt();
    std::vector<usint> map(N);
    PrecomputeAutoMap(N, index, &map);

    Element sPermuted = s.AutomorphismTransform(index, map);
    privateKeyPermuted->SetPrivateElement(sPermuted);

    (*result)[indexList[i]] = MultiKeySwitchGen(
        privateKeyPermuted, privateKey, evalKeyMap->find(indexList[i])->second);
  }

  return result;
}

template <class Element>
shared_ptr<std::map<usint, EvalKey<Element>>>
MultipartyBase<Element>::MultiEvalAtIndexKeyGen(
    const PrivateKey<Element> privateKey,
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap,
    const std::vector<int32_t> &indexList) const {
  const auto cc = privateKey->GetCryptoContext();

  usint M = privateKey->GetCryptoParameters()
      ->GetElementParams()->GetCyclotomicOrder();

  std::vector<uint32_t> autoIndices(indexList.size());

  for (size_t i = 0; i < indexList.size(); i++) {
    autoIndices[i] = (cc->getSchemeId() == "CKKSRNS")
            ? FindAutomorphismIndex2nComplex(indexList[i], M)
            : FindAutomorphismIndex2n(indexList[i], M);
  }

  return MultiEvalAutomorphismKeyGen(privateKey, evalKeyMap, autoIndices);
}

template <class Element>
shared_ptr<std::map<usint, EvalKey<Element>>>
MultipartyBase<Element>::MultiEvalSumKeyGen(
    const PrivateKey<Element> privateKey,
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap) const {
  const auto cryptoParams = privateKey->GetCryptoParameters();

  size_t max = ceil(log2(cryptoParams->GetEncodingParams()->GetBatchSize()));
  std::vector<usint> indices(max);
  usint M = cryptoParams->GetElementParams()->GetCyclotomicOrder();

  usint g = 5;

  for (size_t j = 0; j < max; j++) {
    indices[j] = g;
    g = (g * g) % M;
  }

  return MultiEvalAutomorphismKeyGen(privateKey, evalKeyMap, indices);
}

template <class Element>
Ciphertext<Element> MultipartyBase<Element>::MultipartyDecryptLead(
    ConstCiphertext<Element> ciphertext,
    const PrivateKey<Element> privateKey) const {
  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersRLWE<Element>>(
          privateKey->GetCryptoParameters());

  const shared_ptr<ParmType> elementParams = cryptoParams->GetElementParams();
  const auto ns = cryptoParams->GetNoiseScale();

  const std::vector<Element> &cv = ciphertext->GetElements();

  const Element &s = privateKey->GetPrivateElement();

  DggType dgg(MP_SD);
  Element e(dgg, elementParams, Format::EVALUATION);

  Element b = cv[0] + s * cv[1] + ns * e;
  b.SwitchFormat();

  Ciphertext<Element> result = ciphertext->Clone();
  result->SetElements({std::move(b)});
  return result;
}

template <class Element>
Ciphertext<Element> MultipartyBase<Element>::MultipartyDecryptMain(
    ConstCiphertext<Element> ciphertext,
    const PrivateKey<Element> privateKey) const {
  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersRLWE<Element>>(
          privateKey->GetCryptoParameters());

  const shared_ptr<ParmType> elementParams = cryptoParams->GetElementParams();
  const auto es = cryptoParams->GetNoiseScale();

  const std::vector<Element> &cv = ciphertext->GetElements();
  const Element &s = privateKey->GetPrivateElement();

  DggType dgg(MP_SD);
  Element e(dgg, elementParams, Format::EVALUATION);

  // e is added to do noise flooding
  Element b = s * cv[1] + es * e;

  Ciphertext<Element> result = ciphertext->Clone();
  result->SetElements({std::move(b)});
  return result;
}

template <class Element>
DecryptResult MultipartyBase<Element>::MultipartyDecryptFusion(
    const vector<Ciphertext<Element>> &ciphertextVec,
    NativePoly *plaintext) const {
  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersRLWE<Element>>(
          ciphertextVec[0]->GetCryptoParameters());

  const std::vector<Element> &cv0 = ciphertextVec[0]->GetElements();

  Element b = cv0[0];
  for (size_t i = 1; i < ciphertextVec.size(); i++) {
    const std::vector<Element> &cvi = ciphertextVec[i]->GetElements();
    b += cvi[0];
  }
  b.SetFormat(Format::COEFFICIENT);

  *plaintext = b.ToNativePoly();

  return DecryptResult(plaintext->GetLength());
}

template <class Element>
PublicKey<Element> MultipartyBase<Element>::MultiAddPubKeys(
    PublicKey<Element> publicKey1, PublicKey<Element> publicKey2) const {
  const auto cc = publicKey1->GetCryptoContext();

  PublicKey<Element> publicKeySum =
      std::make_shared<PublicKeyImpl<Element>>(cc);

  const Element &a = publicKey1->GetPublicElements()[1];

  const Element &b1 = publicKey1->GetPublicElements()[0];
  const Element &b2 = publicKey2->GetPublicElements()[0];

  publicKeySum->SetPublicElementAtIndex(0, std::move(b1 + b2));
  publicKeySum->SetPublicElementAtIndex(1, a);

  return publicKeySum;
}

template <class Element>
EvalKey<Element> MultipartyBase<Element>::MultiAddEvalKeys(
    EvalKey<Element> evalKey1, EvalKey<Element> evalKey2) const {
  const auto cc = evalKey1->GetCryptoContext();

  EvalKey<Element> evalKeySum =
      std::make_shared<EvalKeyRelinImpl<Element>>(cc);

  const std::vector<Element> &a = evalKey1->GetAVector();

  const std::vector<Element> &b1 = evalKey1->GetBVector();
  const std::vector<Element> &b2 = evalKey2->GetBVector();

  std::vector<Element> b;

  for (usint i = 0; i < a.size(); i++) {
    b.push_back(b1[i] + b2[i]);
  }

  evalKeySum->SetAVector(a);
  evalKeySum->SetBVector(std::move(b));

  return evalKeySum;
}

template <class Element>
EvalKey<Element> MultipartyBase<Element>::MultiAddEvalMultKeys(
    EvalKey<Element> evalKey1, EvalKey<Element> evalKey2) const {
  const auto cc = evalKey1->GetCryptoContext();

  EvalKey<Element> evalKeySum =
      std::make_shared<EvalKeyRelinImpl<Element>>(cc);

  const std::vector<Element> &a1 = evalKey1->GetAVector();
  const std::vector<Element> &a2 = evalKey2->GetAVector();

  const std::vector<Element> &b1 = evalKey1->GetBVector();
  const std::vector<Element> &b2 = evalKey2->GetBVector();

  std::vector<Element> a;
  std::vector<Element> b;

  for (usint i = 0; i < a1.size(); i++) {
    a.push_back(a1[i] + a2[i]);
    b.push_back(b1[i] + b2[i]);
  }

  evalKeySum->SetAVector(std::move(a));
  evalKeySum->SetBVector(std::move(b));

  return evalKeySum;
}

template <class Element>
EvalKey<Element> MultipartyBase<Element>::MultiMultEvalKey(
    PrivateKey<Element> privateKey, EvalKey<Element> evalKey) const {
  const auto cc = evalKey->GetCryptoContext();

  const auto cryptoParams =
      std::static_pointer_cast<CryptoParametersRLWE<Element>>(
          cc->GetCryptoParameters());

  const DggType &dgg = cryptoParams->GetDiscreteGaussianGenerator();
  const auto elementParams = cryptoParams->GetElementParams();

  EvalKey<Element> evalKeyResult =
      std::make_shared<EvalKeyRelinImpl<Element>>(cc);

  const std::vector<Element> &a0 = evalKey->GetAVector();
  const std::vector<Element> &b0 = evalKey->GetBVector();

  const Element &s = privateKey->GetPrivateElement();
  const auto ns = cryptoParams->GetNoiseScale();

  std::vector<Element> a;
  std::vector<Element> b;

  for (usint i = 0; i < a0.size(); i++) {
    Element e1(dgg, elementParams, Format::EVALUATION);
    Element e2(dgg, elementParams, Format::EVALUATION);

    a.push_back(a0[i] * s + ns * e1);
    b.push_back(b0[i] * s + ns * e2);
  }

  evalKeyResult->SetAVector(std::move(a));
  evalKeyResult->SetBVector(std::move(b));

  return evalKeyResult;
}

template <class Element>
shared_ptr<std::map<usint, EvalKey<Element>>>
MultipartyBase<Element>::MultiAddEvalAutomorphismKeys(
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap1,
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap2) const {
  auto evalKeyMapAuto = std::make_shared<std::map<usint, EvalKey<Element>>>();

  for (auto it = evalKeyMap1->begin(); it != evalKeyMap1->end(); ++it) {
    auto it2 = evalKeyMap2->find(it->first);
    if (it2 != evalKeyMap2->end())
      (*evalKeyMapAuto)[it->first] = MultiAddEvalKeys(it->second, it2->second);
  }

  return evalKeyMapAuto;
}

template <class Element>
shared_ptr<std::map<usint, EvalKey<Element>>>
MultipartyBase<Element>::MultiAddEvalSumKeys(
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap1,
    const shared_ptr<std::map<usint, EvalKey<Element>>> evalKeyMap2) const {
  auto EvalKeyMapSum = std::make_shared<std::map<usint, EvalKey<Element>>>();

  for (auto it = evalKeyMap1->begin(); it != evalKeyMap1->end(); ++it) {
    auto it2 = evalKeyMap2->find(it->first);
    if (it2 != evalKeyMap2->end())
      (*EvalKeyMapSum)[it->first] = MultiAddEvalKeys(it->second, it2->second);
  }

  return EvalKeyMapSum;
}

}  // namespace lbcrypto

#endif
