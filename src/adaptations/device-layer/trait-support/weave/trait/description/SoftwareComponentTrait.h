
/*
 *    Copyright (c) 2019 Google LLC.
 *    Copyright (c) 2016-2018 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/*
 *    THIS FILE IS GENERATED. DO NOT MODIFY.
 *
 *    SOURCE TEMPLATE: trait.cpp.h
 *    SOURCE PROTO: weave/trait/description/software_component_trait.proto
 *
 */
#ifndef _WEAVE_TRAIT_DESCRIPTION__SOFTWARE_COMPONENT_TRAIT_H_
#define _WEAVE_TRAIT_DESCRIPTION__SOFTWARE_COMPONENT_TRAIT_H_

#include <Weave/Profiles/data-management/DataManagement.h>
#include <Weave/Support/SerializationUtils.h>



namespace Schema {
namespace Weave {
namespace Trait {
namespace Description {
namespace SoftwareComponentTrait {

extern const nl::Weave::Profiles::DataManagement::TraitSchemaEngine TraitSchema;

enum {
      kWeaveProfileId = (0x0U << 16) | 0x1eU
};

//
// Properties
//

enum {
    kPropertyHandle_Root = 1,

    //---------------------------------------------------------------------------------------------------------------------------//
    //  Name                                IDL Type                            TLV Type           Optional?       Nullable?     //
    //---------------------------------------------------------------------------------------------------------------------------//

    //
    //  software_components                 repeated SoftwareComponentTypeStruct array             NO              NO
    //
    kPropertyHandle_SoftwareComponents = 2,

    //
    // Enum for last handle
    //
    kLastSchemaHandle = 2,
};

//
// Event Structs
//

struct SoftwareComponentTypeStruct
{
    const char * componentName;
    const char * componentVersion;

    static const nl::SchemaFieldDescriptor FieldSchema;

};

struct SoftwareComponentTypeStruct_array {
    uint32_t num;
    SoftwareComponentTypeStruct *buf;
};

} // namespace SoftwareComponentTrait
} // namespace Description
} // namespace Trait
} // namespace Weave
} // namespace Schema
#endif // _WEAVE_TRAIT_DESCRIPTION__SOFTWARE_COMPONENT_TRAIT_H_
