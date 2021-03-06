#include "program.hpp"

#include <algorithm>
#include <iterator>
#include <array>

bool ProgramData::AttemptReset(Instruction* Begin, Instruction* End)
{
	// Detect a "[-]" and replace it with a Reset instruction

	if (std::distance(Begin, End) == 2
		&& *Begin == Instruction::Type::LoopStart
		&& *std::next(Begin) == Instruction::Type::Addition)
	{
		Text.erase(Text.end() - 2, Text.end());

		if (Text.back() == Instruction::Type::Reset && !Text.back().Offset)
			return true;

		// Detect "[-]>[-]"
		if (Text.back() == Instruction::Type::MovePointer
			&& *std::prev(std::cend(Text), 2) == Instruction::Type::Reset)
		{
			/*
			*	If the pointer is only changed by 1 then the program
			*	is clearing a range, which allows us to combine the instructions.
			*	If not, we can always combine the MovePointer with the Reset instruction.
			*/
			auto MoveAmount = std::abs(Text.back().Value);

			if (MoveAmount == 1)
				std::prev(std::end(Text), 2)->Value += Text.back().Value;

			std::prev(std::end(Text), 2)->Offset += Text.back().Value;

			Text.pop_back();

			if (MoveAmount != 1)
				Text.emplace_back(Instruction::Type::Reset, 1, 0);

			return true;
		}

		Text.emplace_back(Instruction::Type::Reset, 1, 0);
		return true;
	}

	return false;
}

bool ProgramData::AttemptSeek(Instruction* Begin, Instruction* End)
{
	// Detect a "[<]" / "[>]" and replace it with a Seek instruction

	if (std::distance(Begin, End) == 2
		&& *Begin == Instruction::Type::LoopStart
		&& *std::next(Begin) == Instruction::Type::MovePointer)
	{
		auto Data = Text.back().Value;

		Text.erase(Text.end() - 2, Text.end());

		Text.emplace_back(Instruction::Type::Seek, Data);

		return true;
	}

	return false;
}

bool ProgramData::AttemptMultiplication(Instruction* Begin, Instruction* End)
{
	/*
	*	If this loop does not contain other non-optimized loops, Seek's,
	*	and Input/Output instructions, then we should be
	*	able to replace it with Multiplication instructions
	*	and a Push/Pop pair
	*/

	static constexpr std::array<Instruction::Type, 5> Unwanted = { Instruction::Type::LoopStart, Instruction::Type::LoopEnd,
		Instruction::Type::Input, Instruction::Type::Output, Instruction::Type::Seek };

	if (std::find_first_of(std::next(Begin), End, std::cbegin(Unwanted), std::cend(Unwanted)) == End)
	{
		Instruction::value_type CurrentOffset = 0;
		Instruction::value_type LastOffset = 0;

		bool IsLeafLoop = true;

		std::vector<Instruction> Operations;
		Operations.reserve(std::distance(Begin, End));
		Instruction::value_type Cell0Total = 0;

		auto InsertMovePointer = [&]
		{
			if (CurrentOffset - LastOffset)
			{
				Operations.emplace_back(Instruction::Type::MovePointer, CurrentOffset - LastOffset);
				CurrentOffset = LastOffset;
			}
		};

		for (auto Iterator = std::next(Begin); Iterator != End; ++Iterator)
		{
			switch (Iterator->Command)
			{
			case Instruction::Type::MovePointer:
				CurrentOffset += Iterator->Value;
				break;
			case Instruction::Type::Addition:
				if (!CurrentOffset)
				{
					Cell0Total += Iterator->Value;
				}
				else
				{
					Operations.emplace_back(Instruction::Type::Multiplication, Iterator->Value, CurrentOffset - LastOffset);

					LastOffset = CurrentOffset;
				}
				break;
			case Instruction::Type::Multiplication:
				CurrentOffset += Iterator->Offset;

				if (!CurrentOffset)
				{
					// Not yet implemented, needs more experimenting
					return false;
				}
				else
				{
					Operations.emplace_back(Instruction::Type::Multiplication, Iterator->Value, CurrentOffset - LastOffset);

					LastOffset = CurrentOffset;
				}
				break;
			case Instruction::Type::Push:
			case Instruction::Type::Pop:
				IsLeafLoop = false;
			case Instruction::Type::Set:
			case Instruction::Type::Reset:
				InsertMovePointer();
				Operations.push_back(*Iterator);
				break;
			case Instruction::Type::PushFast:
				IsLeafLoop = false;

				InsertMovePointer();
				Operations.emplace_back(Instruction::Type::Push);
				break;
			case Instruction::Type::PopFast:
				IsLeafLoop = false;

				InsertMovePointer();
				Operations.emplace_back(Instruction::Type::Pop);
				break;
			}
		}

		/*
		*	Make sure the Pointer ended up were it started
		*	and only 1 was subtracted from Cell #0
		*/
		if (!CurrentOffset && Cell0Total == -1)
		{
			Text.erase(Text.begin() + std::distance(Text.data(), Begin), Text.end());

			Text.emplace_back(IsLeafLoop ? Instruction::Type::PushFast : Instruction::Type::Push);
			
			for (auto Operation : Operations)
				Text.emplace_back(Operation);

			Text.emplace_back(IsLeafLoop ? Instruction::Type::PopFast : Instruction::Type::Pop);

			Text.emplace_back(Instruction::Type::MovePointer, -LastOffset);

			return true;
		}
	}

	return false;
}

bool ProgramData::DropEmptyLoop(Instruction* Begin, Instruction* End)
{
	if (std::distance(Begin, End) == 1)
	{
		Text.pop_back();
		
		return true;
	}

	return false;
}

void ProgramData::DropPopFast()
{
	Text.erase(std::remove(std::begin(Text), std::end(Text), Instruction::Type::PopFast), std::end(Text));
}
