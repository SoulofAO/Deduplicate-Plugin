// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeduplicateObjects/EqualNeedlemanWunschDataDeduplication.h"
#include "Engine/AssetManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UEqualNeedlemanWunschDataDeduplication::UEqualNeedlemanWunschDataDeduplication()
{
	SimilarityThreshold = 0.7f;
}

float UEqualNeedlemanWunschDataDeduplication::CalculateBinarySimilarity(const TArray<uint8>& Data1, const TArray<uint8>& Data2) const
{
	if (Data1.Num() == 0 || Data2.Num() == 0)
	{
		return 0.0f;
	}

	const int32 LengthA = Data1.Num();
	const int32 LengthB = Data2.Num();


	const int32 Width = LengthB + 1;
	const int32 Height = LengthA + 1;
	const int32 TotalCells = Width * Height;

	const int32 NegInf = INT32_MIN / 4;

	TArray<int32> H;
	TArray<int32> E;
	TArray<int32> F;

	H.Init(NegInf, TotalCells);
	E.Init(NegInf, TotalCells);
	F.Init(NegInf, TotalCells);

	auto Idx = [Width](int32 Row, int32 Col) { return Row * Width + Col; };

	H[Idx(0, 0)] = 0;
	E[Idx(0, 0)] = NegInf;
	F[Idx(0, 0)] = NegInf;

	for (int32 Row = 1; Row <= LengthA; ++Row)
	{
		int32 Index = Idx(Row, 0);
		E[Index] = GapOpen + (Row - 1) * GapExtend;
		H[Index] = NegInf;
		F[Index] = NegInf;
	}

	for (int32 Col = 1; Col <= LengthB; ++Col)
	{
		int32 Index = Idx(0, Col);
		F[Index] = GapOpen + (Col - 1) * GapExtend;
		H[Index] = NegInf;
		E[Index] = NegInf;
	}

	for (int32 Row = 1; Row <= LengthA; ++Row)
	{
		for (int32 Col = 1; Col <= LengthB; ++Col)
		{
			int32 Index = Idx(Row, Col);
			int32 IndexDiag = Idx(Row - 1, Col - 1);
			int32 IndexUp = Idx(Row - 1, Col);
			int32 IndexLeft = Idx(Row, Col - 1);

			// E: пропуск по B (съели символ из A, B имеет '-')
			int32 FromHToE = H[IndexUp] + GapOpen + GapExtend;
			int32 FromEToE = E[IndexUp] + GapExtend;
			E[Index] = (FromHToE > FromEToE) ? FromHToE : FromEToE;

			// F: пропуск по A (съели символ из B, A имеет '-')
			int32 FromHToF = H[IndexLeft] + GapOpen + GapExtend;
			int32 FromFToF = F[IndexLeft] + GapExtend;
			F[Index] = (FromHToF > FromFToF) ? FromHToF : FromFToF;

			// H: диагональ (match/mismatch)
			int32 BestPrev = H[IndexDiag];
			if (E[IndexDiag] > BestPrev) BestPrev = E[IndexDiag];
			if (F[IndexDiag] > BestPrev) BestPrev = F[IndexDiag];

			int32 SubScore = (Data1[Row - 1] == Data2[Col - 1]) ? MatchScore : MismatchScore;
			H[Index] = BestPrev + SubScore;
		}
	}

	// Ќачинаем backtrace с максимума в правом-нижнем углу
	int32 LastIndex = Idx(LengthA, LengthB);
	int32 CurrentMatrix = 0; // 0 = H, 1 = E, 2 = F
	int32 BestEndValue = H[LastIndex];
	if (E[LastIndex] > BestEndValue) { BestEndValue = E[LastIndex]; CurrentMatrix = 1; }
	if (F[LastIndex] > BestEndValue) { BestEndValue = F[LastIndex]; CurrentMatrix = 2; }

	int32 RowPtr = LengthA;
	int32 ColPtr = LengthB;
	int32 MatchingCount = 0;
	int32 AlignmentLength = 0;

	// Backtrace: восстанавливаем выравнивание и считаем совпадени€
	while (RowPtr > 0 || ColPtr > 0)
	{
		if (CurrentMatrix == 0) // H: диагональ
		{
			// ≈сли достигли краЄв Ч переключаемс€ в соответствующую матрицу пропусков
			if (RowPtr == 0 || ColPtr == 0)
			{
				if (RowPtr > 0) CurrentMatrix = 1; // E (двигаемс€ вверх по Row)
				else if (ColPtr > 0) CurrentMatrix = 2; // F (двигаемс€ влево по Col)
				else break;
				continue;
			}

			// “екущий элемент H[Index] = max(H,E,F at diag) + SubScore
			int32 Index = Idx(RowPtr, ColPtr);
			int32 IndexDiag = Idx(RowPtr - 1, ColPtr - 1);
			int32 SubScore = (Data1[RowPtr - 1] == Data2[ColPtr - 1]) ? MatchScore : MismatchScore;

			// ”читываем совпадение/не совпадение
			++AlignmentLength;
			if (Data1[RowPtr - 1] == Data2[ColPtr - 1])
			{
				++MatchingCount;
			}

			// ѕереходим на диагональ
			// ¬ыбираем, откуда пришли: H, E или F на диагонали
			int32 PrevH = H[IndexDiag];
			int32 PrevE = E[IndexDiag];
			int32 PrevF = F[IndexDiag];

			int32 PrevBest = PrevH;
			CurrentMatrix = 0;
			if (PrevE > PrevBest) { PrevBest = PrevE; CurrentMatrix = 1; }
			if (PrevF > PrevBest) { PrevBest = PrevF; CurrentMatrix = 2; }

			RowPtr -= 1;
			ColPtr -= 1;
		}
		else if (CurrentMatrix == 1) // E: вертикальный пропуск (двигаемс€ по Row)
		{
			// ≈сли ColPtr==0, то можем только двигатьс€ по Row до нул€
			if (RowPtr == 0)
			{
				// защищаем от зацикливани€
				break;
			}

			++AlignmentLength;
			// при пропуске совпадений не добавл€ем в MatchingCount
			int32 Index = Idx(RowPtr, ColPtr);
			int32 IndexUp = Idx(RowPtr - 1, ColPtr);

			// ѕровер€ем, откуда пришли: E[Index] = max(H[IndexUp] + GapOpen+GapExtend, E[IndexUp] + GapExtend)
			int32 FromE = E[IndexUp] + GapExtend;
			int32 FromH = H[IndexUp] + GapOpen + GapExtend;

			if (FromE >= FromH)
			{
				CurrentMatrix = 1; // продолжение E
			}
			else
			{
				CurrentMatrix = 0; // пришли из H (открытие пропуска)
			}

			RowPtr -= 1;
		}
		else // CurrentMatrix == 2, F: горизонтальный пропуск (двигаемс€ по Col)
		{
			if (ColPtr == 0)
			{
				break;
			}

			++AlignmentLength;
			int32 Index = Idx(RowPtr, ColPtr);
			int32 IndexLeft = Idx(RowPtr, ColPtr - 1);

			// F[Index] = max(H[IndexLeft] + GapOpen+GapExtend, F[IndexLeft] + GapExtend)
			int32 FromF = F[IndexLeft] + GapExtend;
			int32 FromH = H[IndexLeft] + GapOpen + GapExtend;

			if (FromF >= FromH)
			{
				CurrentMatrix = 2; // продолжение F
			}
			else
			{
				CurrentMatrix = 0; // пришли из H (открытие пропуска)
			}

			ColPtr -= 1;
		}
	}

	// «ащита от делени€ на ноль (в теории не может быть AlignmentLength==0, но на вс€кий случай)
	if (AlignmentLength == 0)
	{
		return 0.0f;
	}

	// ƒол€ совпадающих байтов в выровненной последовательности
	float Similarity = (float)MatchingCount / (float)AlignmentLength;
	// Ќормируем строго в диапазон [0,1]
	if (Similarity < 0.0f) Similarity = 0.0f;
	if (Similarity > 1.0f) Similarity = 1.0f;

	return Similarity;
}
