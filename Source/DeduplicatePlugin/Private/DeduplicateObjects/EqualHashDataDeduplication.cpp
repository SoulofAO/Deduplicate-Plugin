/*
 * Publisher: AO
 * Year of Publication: 2026
 * Copyright AO All Rights Reserved.
 */


#include "DeduplicateObjects/EqualHashDataDeduplication.h"

float UEqualHashDataDeduplication::CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const
{
	const int32 BlockSize = 64;

	auto ComputeBlockHashes = [&](const TArray<uint8>& Data) -> TSet<uint32>
		{
			TSet<uint32> HashSet;
			const int32 DataSize = Data.Num();
			for (int32 Offset = 0; Offset < DataSize; Offset += BlockSize)
			{
				const int32 ChunkSize = FMath::Min(BlockSize, DataSize - Offset);
				const uint32 HashValue = FCrc::MemCrc32(Data.GetData() + Offset, ChunkSize);
				HashSet.Add(HashValue);
			}
			return HashSet;
		};

	TSet<uint32> HashSet1 = ComputeBlockHashes(Data1);
	TSet<uint32> HashSet2 = ComputeBlockHashes(Data2);

	if (HashSet1.Num() == 0 && HashSet2.Num() == 0)
	{
		return 1.0f;
	}

	int32 IntersectionCount = 0;
	for (uint32 HashValue : HashSet1)
	{
		if (HashSet2.Contains(HashValue))
		{
			IntersectionCount++;
		}
	}

	const int32 UnionCount = HashSet1.Num() + HashSet2.Num() - IntersectionCount;
	if (UnionCount == 0)
	{
		return 0.0f;
	}

	const float Similarity = (float)IntersectionCount / (float)UnionCount;
	return Similarity;
}