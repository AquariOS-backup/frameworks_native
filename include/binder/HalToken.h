/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ANDROID_HALTOKEN_H
#define ANDROID_HALTOKEN_H

#include <binder/Parcel.h>
#include <hidl/HidlSupport.h>

/**
 * It is possible to pass a hidl interface via as a binder interface by
 * providing an appropriate "wrapper" class.
 *
 * Terminology:
 * - `HalToken`: The type for a "token" of a hidl interface. This is defined to
 *   be compatible with `ITokenManager.hal`.
 * - `HInterface`: The base type for a hidl interface. Currently, it is defined
 *   as `::android::hidl::base::V1_0::IBase`.
 * - `HALINTERFACE`: The hidl interface that will be sent through binders.
 * - `INTERFACE`: The binder interface that will be the wrapper of
 *   `HALINTERFACE`. `INTERFACE` is supposed to be similar to `HALINTERFACE`.
 *
 * To demonstrate how this is done, here is an example. Suppose `INTERFACE` is
 * `IFoo` and `HALINTERFACE` is `HFoo`. The required steps are:
 * 1. Use DECLARE_HYBRID_META_INTERFACE instead of DECLARE_META_INTERFACE in the
 *    definition of `IFoo`. The usage is
 *        DECLARE_HYBRID_META_INTERFACE(IFoo, HFoo)
 *    inside the body of `IFoo`.
 * 2. Create a converter class that derives from
 *    `H2BConverter<HFoo, IFoo, BnFoo>`. Let us call this `H2BFoo`.
 * 3. Add the following constructors in `H2BFoo` that call the corresponding
 *    constructors in `H2BConverter`:
 *        H2BFoo(const sp<HalInterface>& base) : CBase(base) {}
 *    Note: `CBase = H2BConverter<HFoo, IFoo, BnFoo>` and `HalInterface = HFoo`
 *    are member typedefs of `CBase`, so the above line can be copied verbatim
 *    into `H2BFoo`.
 * 4. Add conversion functions inside `H2BFoo`. `H2BConverter` provides a
 *    protected `mBase` of type `sp<HFoo>` that can be used to access the HFoo
 *    instance. (There is also a public function named `getHalInterface()` that
 *    returns `mBase`.)
 * 5. Create a hardware proxy class that derives from
 *    `HpInterface<BpFoo, H2BFoo>`. Name this class `HpFoo`. (This name cannot
 *    deviate. See step 8 below.)
 * 6. Add the following constructor to `HpFoo`:
 *        HpFoo(const sp<IBinder>& base): PBase(base) {}
 *    Note: `PBase` a member typedef of `HpInterface<BpFoo, H2BFoo>` that is
 *    equal to `HpInterface<BpFoo, H2BFoo>` itself, so the above line can be
 *    copied verbatim into `HpFoo`.
 * 7. Delegate all functions in `HpFoo` that come from `IFoo` except
 *    `getHalToken` and `getHalInterface` to the protected member `mBase`,
 *    which is defined in `HpInterface<BpFoo, H2BFoo>` (hence in `HpFoo`) with
 *    type `IFoo`. (There is also a public function named `getBaseInterface()`
 *    that returns `mBase`.)
 * 8. Replace the existing `IMPLEMENT_META_INTERFACE` for INTERFACE by
 *    `IMPLEMENT_HYBRID_META_INTERFACE`. Note that this macro relies on the
 *    exact naming of `HpFoo`, where `Foo` comes from the interface name `IFoo`.
 *    An example usage is
 *        IMPLEMENT_HYBRID_META_INTERFACE(IFoo, HFoo, "example.interface.foo");
 *
 * `GETTOKEN` Template Argument
 * ============================
 *
 * Following the instructions above, `H2BConverter` and `HpInterface` would use
 * `transact()` to send over tokens, with `code` (the first argument of
 * `transact()`) equal to a 4-byte value of '_GTK'. If this value clashes with
 * other values already in use in the `Bp` class, it can be changed by supplying
 * the last optional template argument to `H2BConverter` and `HpInterface`.
 *
 */

namespace android {

typedef uint64_t HalToken;
typedef ::android::hidl::base::V1_0::IBase HInterface;

sp<HInterface> retrieveHalInterface(const HalToken& token);
bool createHalToken(const sp<HInterface>& interface, HalToken* token);
bool deleteHalToken(const HalToken& token);

template <
        typename HINTERFACE,
        typename INTERFACE,
        typename BNINTERFACE,
        uint32_t GETTOKEN = '_GTK'>
class H2BConverter : public BNINTERFACE {
public:
    typedef H2BConverter<HINTERFACE, INTERFACE, BNINTERFACE, GETTOKEN> CBase;
    typedef INTERFACE BaseInterface;
    typedef HINTERFACE HalInterface;
    static constexpr uint32_t GET_HAL_TOKEN = GETTOKEN;

    H2BConverter(const sp<HalInterface>& base) : mBase(base) {}
    virtual status_t onTransact(uint32_t code,
            const Parcel& data, Parcel* reply, uint32_t flags = 0);
    sp<HalInterface> getHalInterface() override { return mBase; }
    HalInterface* getBaseInterface() { return mBase.get(); }

protected:
    sp<HalInterface> mBase;
};

template <typename BPINTERFACE, typename CONVERTER, uint32_t GETTOKEN = '_GTK'>
class HpInterface : public BPINTERFACE {
public:
    typedef HpInterface<BPINTERFACE, CONVERTER, GETTOKEN> PBase; // Proxy Base
    typedef typename CONVERTER::BaseInterface BaseInterface;
    typedef typename CONVERTER::HalInterface HalInterface;
    static constexpr uint32_t GET_HAL_TOKEN = GETTOKEN;

    explicit HpInterface(const sp<IBinder>& impl);
    sp<HalInterface> getHalInterface() override { return mHal; }
    BaseInterface* getBaseInterface() { return mBase.get(); }

protected:
    sp<BaseInterface> mBase;
    sp<HalInterface> mHal;
};

// ----------------------------------------------------------------------

#define DECLARE_HYBRID_META_INTERFACE(INTERFACE, HAL)                   \
    static const ::android::String16 descriptor;                        \
    static ::android::sp<I##INTERFACE> asInterface(                     \
            const ::android::sp<::android::IBinder>& obj);              \
    virtual const ::android::String16& getInterfaceDescriptor() const;  \
    I##INTERFACE();                                                     \
    virtual ~I##INTERFACE();                                            \
    virtual sp<HAL> getHalInterface();                                  \


#define IMPLEMENT_HYBRID_META_INTERFACE(INTERFACE, HAL, NAME)           \
    const ::android::String16 I##INTERFACE::descriptor(NAME);           \
    const ::android::String16&                                          \
            I##INTERFACE::getInterfaceDescriptor() const {              \
        return I##INTERFACE::descriptor;                                \
    }                                                                   \
    ::android::sp<I##INTERFACE> I##INTERFACE::asInterface(              \
            const ::android::sp<::android::IBinder>& obj)               \
    {                                                                   \
        ::android::sp<I##INTERFACE> intr;                               \
        if (obj != NULL) {                                              \
            intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new Hp##INTERFACE(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }                                                                   \
    I##INTERFACE::I##INTERFACE() { }                                    \
    I##INTERFACE::~I##INTERFACE() { }                                   \
    sp<HAL> I##INTERFACE::getHalInterface() { return nullptr; }         \

// ----------------------------------------------------------------------

template <
        typename HINTERFACE,
        typename INTERFACE,
        typename BNINTERFACE,
        uint32_t GETTOKEN>
status_t H2BConverter<HINTERFACE, INTERFACE, BNINTERFACE, GETTOKEN>::
        onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    if (code == GET_HAL_TOKEN) {
        HalToken token;
        bool result;
        result = createHalToken(mBase, &token);
        if (!result) {
            ALOGE("H2BConverter: Failed to create HAL token.");
        }
        reply->writeBool(result);
        reply->writeUint64(token);
        return NO_ERROR;
    }
    return BNINTERFACE::onTransact(code, data, reply, flags);
}

template <typename BPINTERFACE, typename CONVERTER, uint32_t GETTOKEN>
HpInterface<BPINTERFACE, CONVERTER, GETTOKEN>::HpInterface(
        const sp<IBinder>& impl):
    BPINTERFACE(impl),
    mBase(nullptr) {

    Parcel data, reply;
    data.writeInterfaceToken(BaseInterface::getInterfaceDescriptor());
    if (this->remote()->transact(GET_HAL_TOKEN, data, &reply) == NO_ERROR) {
        bool tokenCreated = reply.readBool();
        HalToken token = reply.readUint64();
        if (!tokenCreated) {
            ALOGE("HpInterface: Sender failed to create HAL token.");
            mBase = this;
        } else {
            sp<HInterface> hInterface = retrieveHalInterface(token);
            deleteHalToken(token);
            if (hInterface != nullptr) {
                mHal = static_cast<HalInterface*>(hInterface.get());
                mBase = new CONVERTER(mHal);
            } else {
                ALOGE("HpInterface: Cannot retrieve HAL interface from token.");
                mBase = this;
            }
        }
    } else {
        mBase = this;
    }
}

// ----------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_HALTOKEN_H