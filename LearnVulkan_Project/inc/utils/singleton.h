#pragma once
#include <cassert>

template <typename T>
class ISingleton
{
private:
	static inline T* mInstance = nullptr;

protected:
	// Private so only the exposed singleton functions can create/destroy our instance
	ISingleton() = default;
	~ISingleton() = default;

public:
	// Delete copy constructor + copy assignment operator
	ISingleton(const ISingleton&) = delete;
	ISingleton& operator = (const ISingleton&) = delete;

	// Delete move constructor + move assignment operator
	ISingleton(ISingleton&&) = delete;
	ISingleton& operator = (ISingleton&&) = delete;

	static void Init() {
		// Don't allow init to be called multiple times
		assert(!mInstance);

		mInstance = new T();
	}

	static void Shutdown() {
		delete mInstance;
		mInstance = nullptr;
	}

	static T& GetInstance() {
		// Ensure instance has been initialized
		assert(mInstance);

		return *mInstance;
	}
};