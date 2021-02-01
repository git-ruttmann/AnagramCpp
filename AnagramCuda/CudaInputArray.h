#pragma once

#include "cuda_runtime.h"

#include<vector>

template<typename T>
class CudaInputArray
{
public:
	CudaInputArray()
		: elemSize(sizeof(T))
	{
		dev_memory = NULL;
		dev_allocated = 0;
	}

	~CudaInputArray()
	{
		cudaFree(dev_memory);
	}

	void Upload()
	{
		if (dev_memory == NULL || dev_allocated < data.size())
		{
			if (dev_memory != NULL)
			{
				cudaFree(dev_memory);
				dev_memory = NULL;
			}

			dev_synced = 0;
			dev_allocated = data.size() + 500;
			cudaMalloc((void**)&dev_memory, dev_allocated * elemSize);
		}

		if (data.size() > 0)
		{
			cudaMemcpy(&dev_memory[dev_synced], &data[dev_synced], (data.size() - dev_synced) * elemSize, cudaMemcpyHostToDevice);
			dev_synced = data.size();
		}
	}

	void Append(T* items, size_t count)
	{
		for (size_t i = 0; i < count; i++)
		{
			data.insert(data.end(), items, items + count);
		}
	}

	/// <summary>
	/// The host side data
	/// </summary>
	std::vector<T> data;

	/// <summary>
	/// The memory in the device
	/// </summary>
	T* dev_memory;
private:
	size_t dev_allocated;
	size_t dev_synced;
	const int elemSize;
};