// @file pre-base.h -- Public key type for lattice crypto operations.
// @author TPOC: contact@palisade-crypto.org
//
// @copyright Copyright (c) 2019, New Jersey Institute of Technology (NJIT)
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution. THIS SOFTWARE IS
// PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef LBCRYPTO_CRYPTO_BASE_CRYPTOPARAMETERS_H
#define LBCRYPTO_CRYPTO_BASE_CRYPTOPARAMETERS_H

#include "utils/serializable.h"
#include "encoding/encodings.h"

/**
 * @namespace lbcrypto
 * The namespace of lbcrypto
 */
namespace lbcrypto {

/**
 * @brief main implementation class to capture essential cryptoparameters of
 * any LBC system
 * @tparam Element a ring element.
 */
template <typename Element>
class CryptoParametersBase : public Serializable {
  using ParmType = typename Element::Params;
  using IntType = typename Element::Integer;
  using DugType = typename Element::DugType;
  using DggType = typename Element::DggType;
  using TugType = typename Element::TugType;

 public:
  CryptoParametersBase() {}

  virtual ~CryptoParametersBase() {}

  /**
   * Returns the value of plaintext modulus p
   *
   * @return the plaintext modulus.
   */
  virtual const PlaintextModulus &GetPlaintextModulus() const {
    return m_encodingParams->GetPlaintextModulus();
  }

  /**
   * Returns the reference to IL params
   *
   * @return the ring element parameters.
   */
  virtual const shared_ptr<typename Element::Params> GetElementParams() const {
    return m_params;
  }

  /**
   * Returns the reference to encoding params
   *
   * @return the encoding parameters.
   */
  virtual const EncodingParams GetEncodingParams() const {
    return m_encodingParams;
  }

  /**
   * Sets the value of plaintext modulus p
   */
  virtual void SetPlaintextModulus(const PlaintextModulus &plaintextModulus) {
    m_encodingParams->SetPlaintextModulus(plaintextModulus);
  }

  virtual bool operator==(const CryptoParametersBase<Element> &cmp) const {
    return *m_encodingParams == *cmp.GetEncodingParams();
  }
  virtual bool operator!=(const CryptoParametersBase<Element> &cmp) const {
    return !(*this == cmp);
  }

  /**
   * Overload to allow printing of parameters to an iostream
   * NOTE that the implementation relies on calling the virtual
   * PrintParameters method
   * @param out - the stream to print to
   * @param item - reference to the item to print
   * @return the stream
   */
  friend std::ostream &operator<<(std::ostream &out,
                                  const CryptoParametersBase &item) {
    item.PrintParameters(out);
    return out;
  }

  virtual usint GetRelinWindow() const { return 0; }

  virtual int GetDepth() const { return 0; }
  virtual size_t GetMaxDepth() const { return 0; }

  virtual const typename Element::DggType &GetDiscreteGaussianGenerator()
      const {
    PALISADE_THROW(config_error, "No DGG Available for this parameter set");
  }

  /**
   * Sets the reference to element params
   */
  virtual void SetElementParams(shared_ptr<typename Element::Params> params) {
    m_params = params;
  }

  /**
   * Sets the reference to encoding params
   */
  virtual void SetEncodingParams(EncodingParams encodingParams) {
    m_encodingParams = encodingParams;
  }

  /////////////////////////////////////
  // SERIALIZATION
  /////////////////////////////////////

  template <class Archive>
  void save(Archive &ar, std::uint32_t const version) const {
    ar(::cereal::make_nvp("elp", m_params));
    ar(::cereal::make_nvp("enp", m_encodingParams));
  }

  template <class Archive>
  void load(Archive &ar, std::uint32_t const version) {
    if (version > SerializedVersion()) {
      PALISADE_THROW(deserialize_error,
                     "serialized object version " + std::to_string(version) +
                         " is from a later version of the library");
    }
    ar(::cereal::make_nvp("elp", m_params));
    ar(::cereal::make_nvp("enp", m_encodingParams));
  }

  std::string SerializedObjectName() const { return "CryptoParametersBase"; }
  static uint32_t SerializedVersion() { return 1; }

 protected:
  explicit CryptoParametersBase(const PlaintextModulus &plaintextModulus) {
    m_encodingParams = std::make_shared<EncodingParamsImpl>(plaintextModulus);
  }

  CryptoParametersBase(shared_ptr<typename Element::Params> params,
                       const PlaintextModulus &plaintextModulus) {
    m_params = params;
    m_encodingParams = std::make_shared<EncodingParamsImpl>(plaintextModulus);
  }

  CryptoParametersBase(shared_ptr<typename Element::Params> params,
                       EncodingParams encodingParams) {
    m_params = params;
    m_encodingParams = encodingParams;
  }

  CryptoParametersBase(CryptoParametersBase<Element> *from,
                       shared_ptr<typename Element::Params> newElemParms) {
    *this = *from;
    m_params = newElemParms;
  }

  virtual void PrintParameters(std::ostream &out) const {
    out << "Element Parameters: " << *m_params << std::endl;
    out << "Encoding Parameters: " << *m_encodingParams << std::endl;
  }

 protected:
  // element-specific parameters
  shared_ptr<typename Element::Params> m_params;

  // encoding-specific parameters
  EncodingParams m_encodingParams;
};

}  // namespace lbcrypto

#endif
