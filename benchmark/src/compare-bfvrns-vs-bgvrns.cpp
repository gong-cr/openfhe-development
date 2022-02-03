/*
 * @file lib-benchmark : library benchmark routines for comparison by build
 * @author TPOC: contact@palisade-crypto.org
 *
 * @copyright Copyright (c) 2019, New Jersey Institute of Technology (NJIT)
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. THIS SOFTWARE IS
 * PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * This file benchmarks a small number of operations in order to exercise large
 * pieces of the library
 */

#define PROFILE
#define _USE_MATH_DEFINES
#include "benchmark/benchmark.h"

#include <iostream>
#include <fstream>
#include <limits>
#include <iterator>
#include <random>

#include "palisade.h"
#include "utils/debug.h"

#include "scheme/bfvrns/cryptocontext-bfvrns.h"
#include "scheme/bgvrns/cryptocontext-bgvrns.h"
#include "gen-cryptocontext.h"

using namespace std;
using namespace lbcrypto;

usint mult_depth = 3;

/*
 * Context setup utility methods
 */
CryptoContext<DCRTPoly> GenerateBFVrnsContext(usint ptm) {
  CCParams<CryptoContextBFVRNS> parameters;
  parameters.SetPlaintextModulus(ptm);
  parameters.SetStandardDeviation(3.19);
  parameters.SetEvalMultCount(mult_depth);
  parameters.SetScalingFactorBits(60);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
  // enable features that you wish to use
  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);
  cc->Enable(ADVANCEDSHE);

  // std::cout << "\nParameters BFVrns for depth " << mult_depth << std::endl;
  // std::cout << "p = " << cc->GetCryptoParameters()->GetPlaintextModulus() <<
  // std::endl; std::cout << "n = " <<
  // cc->GetCryptoParameters()->GetElementParams()->GetCyclotomicOrder() / 2 <<
  // std::endl; std::cout << "log2 q = " <<
  // log2(cc->GetCryptoParameters()->GetElementParams()->GetModulus().ConvertToDouble())
  // << "\n" << std::endl;

  return cc;
}

CryptoContext<DCRTPoly> GenerateBGVrnsContext(usint ptm) {
  CCParams<CryptoContextBGVRNS> parameters;
  parameters.SetMultiplicativeDepth(mult_depth);
  parameters.SetPlaintextModulus(ptm);
  parameters.SetKeySwitchTechnique(BV);
  parameters.SetRescalingTechnique(FIXEDAUTO);

  CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
  cc->Enable(PKE);
  cc->Enable(KEYSWITCH);
  cc->Enable(LEVELEDSHE);
  cc->Enable(ADVANCEDSHE);

  // std::cout << "\nParameters BGVrns for depth " << mult_depth << std::endl;
  // std::cout << "p = " << cc->GetCryptoParameters()->GetPlaintextModulus() <<
  // std::endl; std::cout << "n = " <<
  // cc->GetCryptoParameters()->GetElementParams()->GetCyclotomicOrder() / 2 <<
  // std::endl; std::cout << "log2 q = " <<
  // log2(cc->GetCryptoParameters()->GetElementParams()->GetModulus().ConvertToDouble())
  // << "\n" << std::endl;

  return cc;
}

/*
 * BFVrns benchmarks
 */
void BFVrns_EvalMultManyP2(benchmark::State& state) {
  usint ptm = 2;

  CryptoContext<DCRTPoly> cc = GenerateBFVrnsContext(ptm);

  // KeyGen
  KeyPair<DCRTPoly> keyPair = cc->KeyGen();
  cc->EvalMultKeyGen(keyPair.secretKey);

  std::vector<int64_t> vectorOfInts = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  Plaintext plaintext = cc->MakeCoefPackedPlaintext(vectorOfInts);

  vector<Ciphertext<DCRTPoly>> ciphertexts;
  for (int i = 0; i < (1 << mult_depth); i++)
    ciphertexts.push_back(cc->Encrypt(keyPair.publicKey, plaintext));

  Ciphertext<DCRTPoly> ciphertextMult;
  while (state.KeepRunning()) {
    ciphertextMult = cc->EvalMultMany(ciphertexts);
  }

  Plaintext plaintextDec;
  cc->Decrypt(keyPair.secretKey, ciphertextMult, &plaintextDec);
  plaintextDec->SetLength(plaintext->GetLength());

  if (plaintext != plaintextDec) {
    std::cout << "Original plaintext: " << plaintext << std::endl;
    std::cout << "Evaluated plaintext: " << plaintextDec << std::endl;
  }
}

BENCHMARK(BFVrns_EvalMultManyP2)->Unit(benchmark::kMicrosecond)->MinTime(10.0);

void BGVrns_EvalMultManyP2(benchmark::State& state) {
  usint ptm = 2;

  CryptoContext<DCRTPoly> cc = GenerateBGVrnsContext(ptm);

  // KeyGen
  KeyPair<DCRTPoly> keyPair = cc->KeyGen();
  cc->EvalMultKeyGen(keyPair.secretKey);

  std::vector<int64_t> vectorOfInts = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  Plaintext plaintext = cc->MakeCoefPackedPlaintext(vectorOfInts);

  vector<Ciphertext<DCRTPoly>> ciphertexts;
  for (int i = 0; i < (1 << mult_depth); i++)
    ciphertexts.push_back(cc->Encrypt(keyPair.publicKey, plaintext));

  Ciphertext<DCRTPoly> ciphertextMult;
  while (state.KeepRunning()) {
    ciphertextMult = cc->EvalMultMany(ciphertexts);
  }

  Plaintext plaintextDec;
  cc->Decrypt(keyPair.secretKey, ciphertextMult, &plaintextDec);
  plaintextDec->SetLength(plaintext->GetLength());

  if (plaintext != plaintextDec) {
    std::cout << "Original plaintext: " << plaintext << std::endl;
    std::cout << "Evaluated plaintext: " << plaintextDec << std::endl;
  }
}

BENCHMARK(BGVrns_EvalMultManyP2)->Unit(benchmark::kMicrosecond)->MinTime(10.0);

/*
 * BFVrns benchmarks
 */

void BFVrns_EvalMultManyP65537(benchmark::State& state) {
  usint ptm = 65537;

  CryptoContext<DCRTPoly> cc = GenerateBFVrnsContext(ptm);

  // KeyGen
  KeyPair<DCRTPoly> keyPair = cc->KeyGen();
  cc->EvalMultKeyGen(keyPair.secretKey);

  std::vector<int64_t> vectorOfInts = {1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
  Plaintext plaintext = cc->MakePackedPlaintext(vectorOfInts);

  vector<Ciphertext<DCRTPoly>> ciphertexts;
  for (int i = 0; i < (1 << mult_depth); i++)
    ciphertexts.push_back(cc->Encrypt(keyPair.publicKey, plaintext));

  Ciphertext<DCRTPoly> ciphertextMult;
  while (state.KeepRunning()) {
    ciphertextMult = cc->EvalMultMany(ciphertexts);
  }

  Plaintext plaintextDec;
  cc->Decrypt(keyPair.secretKey, ciphertextMult, &plaintextDec);
  plaintextDec->SetLength(plaintext->GetLength());

  if (plaintext != plaintextDec) {
    std::cout << "Original plaintext: " << plaintext << std::endl;
    std::cout << "Evaluated plaintext: " << plaintextDec << std::endl;
  }
}

BENCHMARK(BFVrns_EvalMultManyP65537)
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(10.0);

void BGVrns_EvalMultManyP65537(benchmark::State& state) {
  usint ptm = 65537;

  CryptoContext<DCRTPoly> cc = GenerateBGVrnsContext(ptm);

  // KeyGen
  KeyPair<DCRTPoly> keyPair = cc->KeyGen();
  cc->EvalMultKeyGen(keyPair.secretKey);

  std::vector<int64_t> vectorOfInts = {1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
  Plaintext plaintext = cc->MakePackedPlaintext(vectorOfInts);

  vector<Ciphertext<DCRTPoly>> ciphertexts;
  for (int i = 0; i < (1 << mult_depth); i++)
    ciphertexts.push_back(cc->Encrypt(keyPair.publicKey, plaintext));

  Ciphertext<DCRTPoly> ciphertextMult;
  while (state.KeepRunning()) {
    ciphertextMult = cc->EvalMultMany(ciphertexts);
  }

  Plaintext plaintextDec;
  cc->Decrypt(keyPair.secretKey, ciphertextMult, &plaintextDec);
  plaintextDec->SetLength(plaintext->GetLength());

  if (plaintext != plaintextDec) {
    std::cout << "Original plaintext: " << plaintext << std::endl;
    std::cout << "Evaluated plaintext: " << plaintextDec << std::endl;
  }
}

BENCHMARK(BGVrns_EvalMultManyP65537)
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(10.0);

BENCHMARK_MAIN();
