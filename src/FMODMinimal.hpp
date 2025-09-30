#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifndef F_CALL
#  if defined(_WIN32)
#    define F_CALL __stdcall
#  else
#    define F_CALL
#  endif
#endif

namespace FMODDynLoad {
unsigned int GetRequiredVersion();
}

namespace FMOD {

using FMOD_RESULT = int;
inline constexpr FMOD_RESULT FMOD_OK = 0;
inline constexpr FMOD_RESULT FMOD_ERR_UNINITIALIZED = 21;

using FMOD_BOOL = int;
inline constexpr FMOD_BOOL FMOD_FALSE = 0;
inline constexpr FMOD_BOOL FMOD_TRUE = 1;

using FMOD_INITFLAGS = std::uint32_t;
using FMOD_STUDIO_INITFLAGS = std::uint32_t;
using FMOD_STUDIO_LOAD_BANK_FLAGS = std::uint32_t;

inline constexpr FMOD_INITFLAGS FMOD_INIT_NORMAL = 0x00000000;
inline constexpr FMOD_STUDIO_INITFLAGS FMOD_STUDIO_INIT_NORMAL = 0x00000000;
inline constexpr FMOD_STUDIO_LOAD_BANK_FLAGS FMOD_STUDIO_LOAD_BANK_NORMAL = 0x00000000;

inline constexpr std::uint32_t FMOD_VERSION_FALLBACK = 0x00020309;

struct FMOD_SYSTEM;
struct FMOD_STUDIO_SYSTEM;
struct FMOD_STUDIO_BANK;
struct FMOD_STUDIO_EVENTDESCRIPTION;
struct FMOD_STUDIO_EVENTINSTANCE;
struct FMOD_STUDIO_BUS;

struct FMOD_GUID {
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    std::uint8_t Data4[8];
};

struct FMOD_STUDIO_PARAMETER_ID {
    std::uint32_t data1;
    std::uint32_t data2;
};

enum FMOD_STUDIO_PARAMETER_TYPE {
    FMOD_STUDIO_PARAMETER_GAME_CONTROLLED = 0,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_DISTANCE,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_EVENT_CONE_ANGLE,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_EVENT_ORIENTATION,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_DIRECTION,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_ELEVATION,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_LISTENER_ORIENTATION,
    FMOD_STUDIO_PARAMETER_AUTOMATIC_SPEED,
};

using FMOD_STUDIO_PARAMETER_FLAGS = std::uint32_t;

enum FMOD_STUDIO_STOP_MODE {
    FMOD_STUDIO_STOP_ALLOWFADEOUT = 0,
    FMOD_STUDIO_STOP_IMMEDIATE = 1,
};

enum FMOD_STUDIO_PLAYBACK_STATE {
    FMOD_STUDIO_PLAYBACK_PLAYING = 0,
    FMOD_STUDIO_PLAYBACK_SUSTAINING = 1,
    FMOD_STUDIO_PLAYBACK_STOPPED = 2,
    FMOD_STUDIO_PLAYBACK_STARTING = 3,
    FMOD_STUDIO_PLAYBACK_STOPPING = 4,
};

struct FMOD_STUDIO_PARAMETER_DESCRIPTION {
    FMOD_STUDIO_PARAMETER_ID id;
    char name[512];
    float minimum;
    float maximum;
    float defaultvalue;
    FMOD_STUDIO_PARAMETER_TYPE type;
    FMOD_STUDIO_PARAMETER_FLAGS flags;
    FMOD_GUID guid;
};

namespace Detail {

inline std::uint32_t GetHeaderVersion() {
    unsigned int version = FMODDynLoad::GetRequiredVersion();
    return version != 0 ? version : FMOD_VERSION_FALLBACK;
}

template <typename HandleType>
class Handle {
  public:
    Handle() = default;
    explicit Handle(HandleType* handle) : handle_(handle) {}

    bool isValid() const { return handle_ != nullptr; }
    void setRaw(HandleType* handle) { handle_ = handle; }
    HandleType* getRaw() const { return handle_; }

  protected:
    HandleType* handle_ = nullptr;
};

class System;
class Bank;
class EventDescription;
class EventInstance;
class Bus;

EventDescription* AcquireEventDescription(FMOD_STUDIO_EVENTDESCRIPTION* handle);
Bus* AcquireBus(FMOD_STUDIO_BUS* handle);
void ClearCaches();

} // namespace Detail

class System : public Detail::Handle<FMOD_SYSTEM> {
  public:
    System() = default;
    explicit System(FMOD_SYSTEM* sys) : Detail::Handle<FMOD_SYSTEM>(sys) {}

    FMOD_RESULT getVersion(unsigned int* version);
    FMOD_RESULT release();
};

namespace Studio {

class System;

class Bank : public Detail::Handle<FMOD_STUDIO_BANK> {
  public:
    Bank() = default;
    explicit Bank(FMOD_STUDIO_BANK* bank) : Detail::Handle<FMOD_STUDIO_BANK>(bank) {}

    FMOD_RESULT loadSampleData();
    FMOD_RESULT unload();
    FMOD_RESULT getEventCount(int* count);
    FMOD_RESULT getEventList(Detail::EventDescription** array, int capacity, int* count);
    FMOD_RESULT getStringCount(int* count);
    FMOD_RESULT getStringInfo(int index, FMOD_GUID* id, char* path, int size, int* retrieved);
};

class EventDescription : public Detail::Handle<FMOD_STUDIO_EVENTDESCRIPTION> {
  public:
    EventDescription() = default;
    explicit EventDescription(FMOD_STUDIO_EVENTDESCRIPTION* event)
        : Detail::Handle<FMOD_STUDIO_EVENTDESCRIPTION>(event) {}

    FMOD_RESULT getPath(char* path, int size, int* retrieved);
    FMOD_RESULT createInstance(Detail::EventInstance** instance);
    FMOD_RESULT getParameterDescriptionCount(int* count);
    FMOD_RESULT getParameterDescriptionByIndex(int index, FMOD_STUDIO_PARAMETER_DESCRIPTION* description);
};

class EventInstance : public Detail::Handle<FMOD_STUDIO_EVENTINSTANCE> {
  public:
    EventInstance() = default;
    explicit EventInstance(FMOD_STUDIO_EVENTINSTANCE* instance)
        : Detail::Handle<FMOD_STUDIO_EVENTINSTANCE>(instance) {}

    FMOD_RESULT start();
    FMOD_RESULT stop(FMOD_STUDIO_STOP_MODE mode);
    FMOD_RESULT release();
    FMOD_RESULT setParameterByName(const char* name, float value, FMOD_BOOL ignoreseekspeed);
    FMOD_RESULT getParameterByName(const char* name, float* value, float* finalvalue);
    FMOD_RESULT getPlaybackState(FMOD_STUDIO_PLAYBACK_STATE* state);
};

class Bus : public Detail::Handle<FMOD_STUDIO_BUS> {
  public:
    Bus() = default;
    explicit Bus(FMOD_STUDIO_BUS* bus) : Detail::Handle<FMOD_STUDIO_BUS>(bus) {}

    FMOD_RESULT stopAllEvents(FMOD_STUDIO_STOP_MODE mode);
};

class System : public Detail::Handle<FMOD_STUDIO_SYSTEM> {
  public:
    System() = default;
    explicit System(FMOD_STUDIO_SYSTEM* system) : Detail::Handle<FMOD_STUDIO_SYSTEM>(system) {}

    static FMOD_RESULT create(System** system);

    FMOD_RESULT initialize(int maxchannels, FMOD_STUDIO_INITFLAGS studioflags, FMOD_INITFLAGS flags, void* extradriverdata);
    FMOD_RESULT getCoreSystem(FMOD::System** coresystem);
    FMOD_RESULT loadBankFile(const char* filename, FMOD_STUDIO_LOAD_BANK_FLAGS flags, Bank** bank);
    FMOD_RESULT getEvent(const char* pathOrID, EventDescription** event);
    FMOD_RESULT getBus(const char* pathOrID, Bus** bus);
    FMOD_RESULT setParameterByName(const char* name, float value, FMOD_BOOL ignoreseekspeed);
    FMOD_RESULT update();
    FMOD_RESULT release();

  private:
    FMOD::System coreSystemWrapper_;
    std::unordered_map<FMOD_STUDIO_BUS*, std::unique_ptr<Bus>> busCache_;
};

} // namespace Studio

} // namespace FMOD

#ifdef FMOD_MINIMAL_IMPLEMENTATION
#error "FMOD_MINIMAL_IMPLEMENTATION should not be defined"
#endif

#ifndef FMOD_MINIMAL_INLINE_IMPL
#define FMOD_MINIMAL_INLINE_IMPL

namespace FMOD {

namespace Detail {

inline std::unordered_map<FMOD_STUDIO_EVENTDESCRIPTION*, std::unique_ptr<Studio::EventDescription>>& EventCache() {
    static std::unordered_map<FMOD_STUDIO_EVENTDESCRIPTION*, std::unique_ptr<Studio::EventDescription>> cache;
    return cache;
}

inline std::unordered_map<FMOD_STUDIO_BUS*, std::unique_ptr<Studio::Bus>>& BusCache() {
    static std::unordered_map<FMOD_STUDIO_BUS*, std::unique_ptr<Studio::Bus>> cache;
    return cache;
}

inline std::mutex& CacheMutex() {
    static std::mutex mutex;
    return mutex;
}

inline EventDescription* AcquireEventDescription(FMOD_STUDIO_EVENTDESCRIPTION* handle) {
    if (!handle) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(CacheMutex());
    auto& cache = EventCache();
    auto it = cache.find(handle);
    if (it == cache.end()) {
        auto entry = std::make_unique<Studio::EventDescription>(handle);
        Studio::EventDescription* ptr = entry.get();
        cache.emplace(handle, std::move(entry));
        return ptr;
    }
    it->second->setRaw(handle);
    return it->second.get();
}

inline Studio::Bus* AcquireBus(FMOD_STUDIO_BUS* handle) {
    if (!handle) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(CacheMutex());
    auto& cache = BusCache();
    auto it = cache.find(handle);
    if (it == cache.end()) {
        auto entry = std::make_unique<Studio::Bus>(handle);
        Studio::Bus* ptr = entry.get();
        cache.emplace(handle, std::move(entry));
        return ptr;
    }
    it->second->setRaw(handle);
    return it->second.get();
}

inline void ClearCaches() {
    std::lock_guard<std::mutex> lock(CacheMutex());
    EventCache().clear();
    BusCache().clear();
}

} // namespace Detail

inline FMOD_RESULT System::getVersion(unsigned int* version) {
    return FMOD_System_GetVersion(this->getRaw(), version);
}

inline FMOD_RESULT System::release() {
    FMOD_RESULT result = FMOD_System_Release(this->getRaw());
    if (result == FMOD_OK) {
        this->setRaw(nullptr);
        delete this;
    }
    return result;
}

namespace Studio {

inline FMOD_RESULT System::create(System** system) {
    if (!system) {
        return FMOD_OK;
    }
    if (*system == nullptr) {
        *system = new System();
    }
    FMOD_STUDIO_SYSTEM* raw = nullptr;
    FMOD_RESULT result = FMOD_Studio_System_Create(&raw, Detail::GetHeaderVersion());
    if (result != FMOD_OK) {
        delete *system;
        *system = nullptr;
        return result;
    }
    (*system)->setRaw(raw);
    return result;
}

inline FMOD_RESULT System::initialize(int maxchannels, FMOD_STUDIO_INITFLAGS studioflags, FMOD_INITFLAGS flags,
                                      void* extradriverdata) {
    return FMOD_Studio_System_Initialize(this->getRaw(), maxchannels, studioflags, flags, extradriverdata);
}

inline FMOD_RESULT System::getCoreSystem(FMOD::System** coresystem) {
    if (!coresystem) {
        return FMOD_OK;
    }
    FMOD_SYSTEM* core = nullptr;
    FMOD_RESULT result = FMOD_Studio_System_GetCoreSystem(this->getRaw(), &core);
    if (result == FMOD_OK) {
        this->coreSystemWrapper_.setRaw(core);
        *coresystem = &this->coreSystemWrapper_;
    }
    return result;
}

inline FMOD_RESULT System::loadBankFile(const char* filename, FMOD_STUDIO_LOAD_BANK_FLAGS flags, Bank** bank) {
    if (!bank) {
        return FMOD_OK;
    }
    FMOD_STUDIO_BANK* raw = nullptr;
    FMOD_RESULT result = FMOD_Studio_System_LoadBankFile(this->getRaw(), filename, flags, &raw);
    if (result != FMOD_OK) {
        *bank = nullptr;
        return result;
    }
    *bank = new Bank(raw);
    return result;
}

inline FMOD_RESULT System::getEvent(const char* pathOrID, EventDescription** event) {
    if (!event) {
        return FMOD_OK;
    }
    FMOD_STUDIO_EVENTDESCRIPTION* raw = nullptr;
    FMOD_RESULT result = FMOD_Studio_System_GetEvent(this->getRaw(), pathOrID, &raw);
    if (result == FMOD_OK) {
        *event = Detail::AcquireEventDescription(raw);
    } else {
        *event = nullptr;
    }
    return result;
}

inline FMOD_RESULT System::getBus(const char* pathOrID, Bus** bus) {
    if (!bus) {
        return FMOD_OK;
    }
    FMOD_STUDIO_BUS* raw = nullptr;
    FMOD_RESULT result = FMOD_Studio_System_GetBus(this->getRaw(), pathOrID, &raw);
    if (result == FMOD_OK) {
        auto it = busCache_.find(raw);
        if (it == busCache_.end()) {
            auto entry = std::make_unique<Bus>(raw);
            *bus = entry.get();
            busCache_.emplace(raw, std::move(entry));
        } else {
            it->second->setRaw(raw);
            *bus = it->second.get();
        }
    } else {
        *bus = nullptr;
    }
    return result;
}

inline FMOD_RESULT System::setParameterByName(const char* name, float value, FMOD_BOOL ignoreseekspeed) {
    return FMOD_Studio_System_SetParameterByName(this->getRaw(), name, value, ignoreseekspeed);
}

inline FMOD_RESULT System::update() {
    return FMOD_Studio_System_Update(this->getRaw());
}

inline FMOD_RESULT System::release() {
    FMOD_RESULT result = FMOD_Studio_System_Release(this->getRaw());
    if (result == FMOD_OK) {
        this->setRaw(nullptr);
        delete this;
    }
    return result;
}

inline FMOD_RESULT Bank::loadSampleData() {
    return FMOD_Studio_Bank_LoadSampleData(this->getRaw());
}

inline FMOD_RESULT Bank::unload() {
    FMOD_RESULT result = FMOD_Studio_Bank_Unload(this->getRaw());
    if (result == FMOD_OK) {
        delete this;
    }
    return result;
}

inline FMOD_RESULT Bank::getEventCount(int* count) {
    return FMOD_Studio_Bank_GetEventCount(this->getRaw(), count);
}

inline FMOD_RESULT Bank::getEventList(EventDescription** array, int capacity, int* count) {
    if (!array || capacity <= 0) {
        return FMOD_OK;
    }
    std::vector<FMOD_STUDIO_EVENTDESCRIPTION*> handles(static_cast<std::size_t>(capacity), nullptr);
    FMOD_RESULT result = FMOD_Studio_Bank_GetEventList(this->getRaw(), handles.data(), capacity, count);
    if (result != FMOD_OK) {
        return result;
    }
    int returned = count ? *count : capacity;
    for (int i = 0; i < returned && i < capacity; ++i) {
        array[i] = Detail::AcquireEventDescription(handles[static_cast<std::size_t>(i)]);
    }
    return result;
}

inline FMOD_RESULT Bank::getStringCount(int* count) {
    return FMOD_Studio_Bank_GetStringCount(this->getRaw(), count);
}

inline FMOD_RESULT Bank::getStringInfo(int index, FMOD_GUID* id, char* path, int size, int* retrieved) {
    return FMOD_Studio_Bank_GetStringInfo(this->getRaw(), index, id, path, size, retrieved);
}

inline FMOD_RESULT EventDescription::getPath(char* path, int size, int* retrieved) {
    return FMOD_Studio_EventDescription_GetPath(this->getRaw(), path, size, retrieved);
}

inline FMOD_RESULT EventDescription::createInstance(EventInstance** instance) {
    if (!instance) {
        return FMOD_OK;
    }
    FMOD_STUDIO_EVENTINSTANCE* raw = nullptr;
    FMOD_RESULT result = FMOD_Studio_EventDescription_CreateInstance(this->getRaw(), &raw);
    if (result == FMOD_OK) {
        *instance = new EventInstance(raw);
    } else {
        *instance = nullptr;
    }
    return result;
}

inline FMOD_RESULT EventDescription::getParameterDescriptionCount(int* count) {
    return FMOD_Studio_EventDescription_GetParameterDescriptionCount(this->getRaw(), count);
}

inline FMOD_RESULT EventDescription::getParameterDescriptionByIndex(int index,
                                                                    FMOD_STUDIO_PARAMETER_DESCRIPTION* description) {
    return FMOD_Studio_EventDescription_GetParameterDescriptionByIndex(this->getRaw(), index, description);
}

inline FMOD_RESULT EventInstance::start() {
    return FMOD_Studio_EventInstance_Start(this->getRaw());
}

inline FMOD_RESULT EventInstance::stop(FMOD_STUDIO_STOP_MODE mode) {
    return FMOD_Studio_EventInstance_Stop(this->getRaw(), mode);
}

inline FMOD_RESULT EventInstance::release() {
    FMOD_RESULT result = FMOD_Studio_EventInstance_Release(this->getRaw());
    if (result == FMOD_OK) {
        delete this;
    }
    return result;
}

inline FMOD_RESULT EventInstance::setParameterByName(const char* name, float value, FMOD_BOOL ignoreseekspeed) {
    return FMOD_Studio_EventInstance_SetParameterByName(this->getRaw(), name, value, ignoreseekspeed);
}

inline FMOD_RESULT EventInstance::getParameterByName(const char* name, float* value, float* finalvalue) {
    return FMOD_Studio_EventInstance_GetParameterByName(this->getRaw(), name, value, finalvalue);
}

inline FMOD_RESULT EventInstance::getPlaybackState(FMOD_STUDIO_PLAYBACK_STATE* state) {
    return FMOD_Studio_EventInstance_GetPlaybackState(this->getRaw(), state);
}

inline FMOD_RESULT Bus::stopAllEvents(FMOD_STUDIO_STOP_MODE mode) {
    return FMOD_Studio_Bus_StopAllEvents(this->getRaw(), mode);
}

} // namespace Studio

} // namespace FMOD

#endif // FMOD_MINIMAL_INLINE_IMPL
