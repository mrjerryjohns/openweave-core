/*
 *
 *    Copyright (c) 2016-2017 Nest Labs, Inc.
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
#ifndef _WEAVE_DATA_MANAGEMENT_PROFILE_DATABASE_H
#define _WEAVE_DATA_MANAGEMENT_PROFILE_DATABASE_H

#include <Weave/Profiles/data-management/WdmManagedNamespace.h>

#if WEAVE_CONFIG_DATA_MANAGEMENT_NAMESPACE == kWeaveManagedNamespace_Legacy
#include <Weave/Profiles/data-management/Legacy/ProfileDatabase.h>
#else
#error "WEAVE_CONFIG_DATA_MANAGEMENT_NAMESPACE defined, but not as namespace kWeaveManagedNamespace_Legacy"
#endif // WEAVE_CONFIG_DATA_MANAGEMENT_NAMESPACE == kWeaveManagedNamespace_Legacy

#endif // _WEAVE_DATA_MANAGEMENT_PROFILE_DATABASE_H
