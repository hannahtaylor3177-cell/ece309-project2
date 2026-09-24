# Design Log — Project 2
(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost
For my Conversation class, I used a dynamically allocated array of Message objects. The conversation starts with a capacity of 0, and when the first message is added, the capacity becomes 1. Whenever the array becomes full, I create a new array with twice the old capacity, move the existing messages into it, delete the old array, and update the pointer and capacity. Therefore, the capacity grows as 1, 2, 4, 8, 16, and so on. Doubling the capacity means that the array does not have to be reallocated every time a message is appended. Although an individual append can take O(n) time when the array needs to grow, most appends only take O(1) time. This gives an amortized O(1) cost per append.


## Rule of Five evidence
I implemented all five special member functions for Conversation. The destructor uses delete[] to release the dynamically allocated message array. The copy constructor and copy assignment operator both perform deep copies. They allocate their own array and copy each message instead of making two conversations point to the same memory. The copy assignment operator also checks for self-assignment before replacing the existing data.

For moving, the move constructor and move assignment operator steal the existing array pointer instead of copying every message. After the pointer is transferred, the source conversation is reset to nullptr with a size and capacity of zero. The move assignment operator also deletes the destination's old array before taking ownership of the source array. My RuleOfFiveCopy and RuleOfFiveMove tests check that copies have different array addresses while moves preserve the original array address and reset the moved-from object.


## Sentinel scanner: bounded pending_ proof
The scanner uses a pending_ string to hold characters that could still become part of <|end_conversation|>. Each call to feed() combines the previous pending characters with the new chunk and first searches for the complete sentinel. If the sentinel is found, the text before it is returned and the sentinel is never included in the output.

If the sentinel is not found, the code will search for the longest suffix of the combined string that is also a prefix of the sentinel. Only this possible partial sentinel is stored in pending_. Since the longest possible proper prefix of the sentinel has length sentinel.size() - 1, pending_ can never contain more than that many characters. Everything before this suffix is safe to emit. This also allows normal text before a partial sentinel to be emitted instead of unnecessarily keeping it in memory. The tests include splitting the sentinel at every possible boundary and a 4 MB adversarial stream to check that the scanner continues working without growing its pending storage with the entire input.


## What I would change differently
If I were doing the project again, I would spend more time planning the sentinel scanner before writing the implementation. My first approach kept the last sentinel.size() - 1 characters regardless of whether they could actually be part of the sentinel. This caused normal text such as "Goodbye." to be held back unnecessarily. I changed the implementation to find the longest suffix that matches a prefix of the sentinel, which makes the scanner more precise while still keeping the pending storage bounded.

I would also write more of the tests earlier in the project. The tests helped me catch the scanner issue and verify the Rule of Five behavior. 
