#pragma once

namespace Utils {
	namespace BufferUtils {
		uint32_t FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties);
		void CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory);
		void CopyBuffer(VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size);

		struct Ssbo
		{
		private:
			VkBuffer mBuffer;
			VkDeviceMemory mDeviceMemory;
			VkDeviceSize mBufferSize;
			void* mPersistentMapping; // points to GPU-side memory

		public:
			void Init(VkDeviceSize _size);
			void Reset();
			void WriteToDescriptorSet(const VkDescriptorSet& _set, uint32_t _binding);

			template<typename T>
			const size_t Size() const {
				return mBufferSize / sizeof(T);
			}

			template<typename T>
			T& GetElement(size_t _index) const {
				assert(_index < Size<T>());
				return reinterpret_cast<T*>(mPersistentMapping)[_index];
			}
		};
	}
}