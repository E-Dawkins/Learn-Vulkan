#pragma once
#include <cassert>

template <typename T>
class ISingleton
{
private:
	static inline ISingleton* mInstance = nullptr;

protected:
	// Protected so only the exposed singleton functions can create/destroy our instance
	// ... but sub-classes can still override with their own constructor/destructor
	ISingleton() = default;
	virtual ~ISingleton() = default;

	virtual void OnInitialized() {}
	virtual void OnCleanup() {}

public:
	// Delete copy constructor + copy assignment operator
	ISingleton(const ISingleton&) = delete;
	ISingleton& operator = (const ISingleton&) = delete;

	// Delete move constructor + move assignment operator
	ISingleton(ISingleton&&) = delete;
	ISingleton& operator = (ISingleton&&) = delete;

	static void Init(auto&&... args) {
		// Don't allow init to be called multiple times
		assert(!mInstance);

		// The forwarding of args was taken directly from: https://en.cppreference.com/cpp/utility/forward
		// still not 100% sure why this works...

		mInstance = new T(std::forward<decltype(args)>(args)...);
		mInstance->OnInitialized();
	}

	static void Shutdown() {
		mInstance->OnCleanup();

		delete mInstance;
		mInstance = nullptr;
	}

	static T& GetInstance() {
		// Ensure instance has been initialized
		assert(mInstance);

		return *dynamic_cast<T*>(mInstance);
	}
};