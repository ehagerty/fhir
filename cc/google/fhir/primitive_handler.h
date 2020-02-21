/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_FHIR_PRIMITIVE_HANDLER_H_
#define GOOGLE_FHIR_PRIMITIVE_HANDLER_H_

#include <memory>
#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/time/time.h"
#include "google/fhir/primitive_wrapper.h"
#include "google/fhir/proto_util.h"
#include "google/fhir/status/status.h"
#include "google/fhir/status/statusor.h"
#include "google/fhir/util.h"
#include "include/json/json.h"
#include "tensorflow/core/lib/core/errors.h"

namespace google {
namespace fhir {

struct JsonPrimitive {
  std::string value;
  std::unique_ptr<::google::protobuf::Message> element;

  const bool is_non_null() const { return value != "null"; }
};

// Class that abstracts direct interaction with primitives.  By delegating
// primitive handling to an instance of this class, core libraries can be
// written without depending on any specific version of FHIR. This allows for
// creating primitives of a given FHIR type (e.g, Decimal), extracting the value
// from a known type, validating primitives, and Parsing and Wrapping functions
// for use by json_format.h
class PrimitiveHandler {
 public:
  virtual ~PrimitiveHandler() {}

  Status ParseInto(const Json::Value& json, const absl::TimeZone tz,
                   ::google::protobuf::Message* target) const;

  Status ParseInto(const Json::Value& json, ::google::protobuf::Message* target) const;

  StatusOr<JsonPrimitive> WrapPrimitiveProto(
      const ::google::protobuf::Message& proto) const;

  Status ValidatePrimitive(const ::google::protobuf::Message& primitive) const;

  virtual StatusOr<std::string> GetStringValue(
      const ::google::protobuf::Message& primitive) const = 0;

  virtual ::google::protobuf::Message* NewString(const std::string& str) const = 0;

  virtual const ::google::protobuf::Descriptor* StringDescriptor() const = 0;

  virtual StatusOr<bool> GetBooleanValue(
      const ::google::protobuf::Message& primitive) const = 0;

  virtual ::google::protobuf::Message* NewBoolean(const bool value) const = 0;

  virtual const ::google::protobuf::Descriptor* BooleanDescriptor() const = 0;

  virtual StatusOr<int> GetIntegerValue(
      const ::google::protobuf::Message& primitive) const = 0;

  virtual ::google::protobuf::Message* NewInteger(const int value) const = 0;

  virtual const ::google::protobuf::Descriptor* IntegerDescriptor() const = 0;

  virtual StatusOr<std::string> GetDecimalValue(
      const ::google::protobuf::Message& primitive) const = 0;

  virtual ::google::protobuf::Message* NewDecimal(const std::string value) const = 0;

  virtual const ::google::protobuf::Descriptor* DecimalDescriptor() const = 0;

 protected:
  PrimitiveHandler(proto::FhirVersion version) : version_(version) {}

  virtual StatusOr<std::unique_ptr<primitives_internal::PrimitiveWrapper>>
  GetWrapper(const ::google::protobuf::Descriptor* target_descriptor) const = 0;

  Status CheckVersion(const ::google::protobuf::Message& message) const;
  Status CheckVersion(const ::google::protobuf::Descriptor* descriptor) const;

  const proto::FhirVersion version_;
};

namespace primitives_internal {

template <typename Expected>
ABSL_MUST_USE_RESULT
Status CheckType(const ::google::protobuf::Message& message) {
  return IsMessageType<Expected>(message)
             ? Status::OK()
             : InvalidArgument("Tried to get ",
                               Expected::descriptor()->full_name(),
                               " value, but message was of type ",
                               message.GetDescriptor()->full_name());
}

// Template for a PrimitiveHandler tied to a single version of FHIR.
// This provides much of the functionality that is common between versions.
// The templating is kept separate from the main interface, in order to provide
// a version-agnostic type that libraries can use without needing to template
// themselves.
template <typename ExtensionType,
          typename StringType = FHIR_DATATYPE(ExtensionType, string_value),
          typename IntegerType = FHIR_DATATYPE(ExtensionType, integer),
          typename DecimalType = FHIR_DATATYPE(ExtensionType, decimal),
          typename BooleanType = FHIR_DATATYPE(ExtensionType, boolean)>
class PrimitiveHandlerTemplate : public PrimitiveHandler {
 public:
  typedef ExtensionType Extension;
  typedef StringType String;
  typedef BooleanType Boolean;
  typedef IntegerType Integer;
  typedef DecimalType Decimal;

  StatusOr<std::string> GetStringValue(
      const ::google::protobuf::Message& primitive) const override {
    FHIR_RETURN_IF_ERROR(CheckType<String>(primitive));
    return dynamic_cast<const String&>(primitive).value();
  }

  ::google::protobuf::Message* NewString(const std::string& str) const override {
    String* msg = new String();
    msg->set_value(str);
    return msg;
  }

  const ::google::protobuf::Descriptor* StringDescriptor() const override {
    return String::GetDescriptor();
  }

  StatusOr<bool> GetBooleanValue(
      const ::google::protobuf::Message& primitive) const override {
    FHIR_RETURN_IF_ERROR(CheckType<Boolean>(primitive));
    return dynamic_cast<const Boolean&>(primitive).value();
  }

  ::google::protobuf::Message* NewBoolean(const bool value) const override {
    Boolean* msg = new Boolean();
    msg->set_value(value);
    return msg;
  }

  const ::google::protobuf::Descriptor* BooleanDescriptor() const override {
    return Boolean::GetDescriptor();
  }

  StatusOr<int> GetIntegerValue(
      const ::google::protobuf::Message& primitive) const override {
    FHIR_RETURN_IF_ERROR(CheckType<Integer>(primitive));
    return dynamic_cast<const Integer&>(primitive).value();
  }

  ::google::protobuf::Message* NewInteger(const int value) const override {
    Integer* msg = new Integer();
    msg->set_value(value);
    return msg;
  }

  const ::google::protobuf::Descriptor* IntegerDescriptor() const override {
    return Integer::GetDescriptor();
  }

  StatusOr<std::string> GetDecimalValue(
      const ::google::protobuf::Message& primitive) const override {
    FHIR_RETURN_IF_ERROR(CheckType<Decimal>(primitive));
    return dynamic_cast<const Decimal&>(primitive).value();
  }

  ::google::protobuf::Message* NewDecimal(const std::string value) const override {
    Decimal* msg = new Decimal();
    msg->set_value(value);
    return msg;
  }

  const ::google::protobuf::Descriptor* DecimalDescriptor() const override {
    return Decimal::GetDescriptor();
  }

 protected:
  PrimitiveHandlerTemplate()
      : PrimitiveHandler(GetFhirVersion(ExtensionType::descriptor())) {}
};

}  // namespace primitives_internal

}  // namespace fhir
}  // namespace google

#endif  // GOOGLE_FHIR_PRIMITIVE_HANDLER_H_
