-- What to do about a cppcheck finding: a short recommendation for the common
-- ids, shown under the message in the S popup (scanners.lua puts it in the
-- finding, diagnostics.lua shows it as "Recommendation:"). Names from the
-- message ('p', 'm_nCount') are put into the advice, so it reads as being
-- about your code. Ids not listed here get no recommendation; with local
-- model checks on, the popup's "Ask the model" asks for one instead.

local M = {}

-- %1, %2: the first and second quoted names in cppcheck's message (or the
-- name after its last colon).
local ADVICE = {
  nullPointer = "Check %1 for null before using it, or make sure every path that reaches this line sets it.",
  nullPointerRedundantCheck = "%1 is used here but compared with null elsewhere: either the check is unneeded (drop it) or this line needs the same check.",
  nullPointerArithmetic = "Pointer arithmetic on a null %1: check it before offsetting.",
  ctunullpointer = "A caller can pass null as %1: check it at the top of the function, or document and assert that it's never null.",
  uninitvar = "Give %1 a value where it's declared (e.g. = 0, = nullptr, {}), so no path reads it uninitialized.",
  uninitdata = "The memory %1 points to is read before it's written: initialize it (calloc, memset, or {}), or write it first.",
  uninitMemberVar = "Initialize %1 in the constructor's initializer list (or give it a default in the class).",
  uninitMemberVarPrivate = "Initialize %1 in the constructor's initializer list (or give it a default in the class).",
  uninitStructMember = "Set %1 before the struct is used, or zero the whole struct when it's declared (= {}).",
  memleak = "%1 is allocated but never freed on this path: free/delete it before returning, or hold it in a std::unique_ptr.",
  memleakOnRealloc = "If realloc fails, %1 is lost: assign the result to a temporary, check it, then assign back.",
  resourceLeak = "%1 (a file or handle) isn't closed on this path: close it before every return, or wrap it in an RAII holder.",
  leakReturnValNotUsed = "The return value is allocated memory and it's dropped: keep it and free it, or don't call the function.",
  deallocDealloc = "%1 is freed twice: set it to nullptr after the first free, or remove one of them.",
  doubleFree = "%1 is freed twice: set it to nullptr after the first free, or remove one of them.",
  deallocuse = "%1 is used after it's freed: move the use above the free, or don't free it yet.",
  mismatchAllocDealloc = "%1 is allocated and freed with different functions: pair new/delete, new[]/delete[], malloc/free.",
  arrayIndexOutOfBounds = "The index goes past the end of %1: check the loop bound or the index against the array size (sizeof / std::size).",
  arrayIndexOutOfBoundsCond = "The index can go past the end of %1 when the condition holds: check it against the array size first.",
  bufferAccessOutOfBounds = "This reads or writes past the end of %1: use its real size (sizeof on the array, or the length you allocated) as the limit.",
  negativeIndex = "%1 can be indexed with a negative value: check the index is >= 0 first.",
  outOfBounds = "The index or size is out of range here: check it against the container's size first.",
  containerOutOfBounds = "%1 can be indexed past its end (it may be empty): check size() first, or use at() to get an exception.",
  stlOutOfBounds = "%1 can be indexed past its end: compare the index with size() (< not <=).",
  bufferNotZeroTerminated = "The buffer may not end in '\\0' after this call: terminate it yourself, or use a function that always does (snprintf, strlcpy).",
  strncatUsage = "strncat's size is the room left, not the buffer size: use sizeof(buf) - strlen(buf) - 1, or snprintf.",
  invalidPrintfArgType_sint = "The printf format doesn't match the argument's type: use the matching specifier (%%ld, %%zu, %%u...).",
  invalidPrintfArgType_uint = "The printf format doesn't match the argument's type: use the matching specifier (%%ld, %%zu, %%u...).",
  invalidPrintfArgType_s = "%%s needs a char*: pass .c_str() for a std::string, or fix the argument.",
  wrongPrintfScanfArgNum = "The number of arguments doesn't match the format string: add or remove one so they line up.",
  invalidScanfArgType_int = "scanf needs a pointer of the type the format says: check the specifier and pass &variable.",
  passedByValue = "Pass %1 by const reference (const T&) instead of by value; it's copied on every call.",
  passedByValueCallback = "Pass %1 by const reference (const T&) instead of by value; it's copied on every call.",
  constParameter = "%1 isn't changed here: declare it const (const T* / const T&).",
  constParameterPointer = "%1 isn't changed through: make it a pointer to const (const T*).",
  constParameterReference = "%1 isn't changed: make it a const reference (const T&).",
  constVariable = "%1 is never changed after it's set: declare it const.",
  constVariablePointer = "%1 isn't changed through: make it a pointer to const.",
  constVariableReference = "%1 isn't changed: make it a const reference.",
  functionConst = "%1 doesn't change the object: mark it const (after the parameter list).",
  functionStatic = "%1 doesn't use the object: it can be static.",
  knownConditionTrueFalse = "This condition always has the same value here: remove the dead branch, or check whether the value is set wrong earlier.",
  duplicateCondition = "The same condition is checked twice: merge the two ifs, or fix the one that should test something else.",
  identicalConditionAfterEarlyExit = "This condition can't be true here (the same check returned earlier): remove it or fix the earlier one.",
  duplicateExpression = "Both sides of the operator are the same: one of them is probably meant to be something else.",
  duplicateBranch = "Both branches do the same thing: drop the if, or fix the branch that should differ.",
  oppositeInnerCondition = "This inner condition is the opposite of the outer one, so it never runs: fix one of them.",
  redundantAssignment = "%1 is assigned and then assigned again before being read: remove the first assignment.",
  redundantInitialization = "%1's initial value is overwritten before it's read: drop the initializer or the later assignment.",
  unreadVariable = "%1 is assigned but never read: remove it, or use it where you meant to.",
  unusedVariable = "%1 is never used: remove it.",
  unusedStructMember = "%1 is never used: remove it, or it's read somewhere cppcheck can't see.",
  unusedFunction = "%1 is never called: remove it, or mark it as used if something outside calls it.",
  variableScope = "%1 can be declared in a narrower scope, where it's used.",
  shadowVariable = "%1 hides another variable of the same name: rename one of them.",
  shadowVar = "%1 hides another variable of the same name: rename one of them.",
  shadowArgument = "%1 hides a parameter of the same name: rename the local.",
  shadowFunction = "%1 hides a function of the same name: rename the variable.",
  noExplicitConstructor = "Mark the one-argument constructor explicit, so %1 isn't converted to by accident.",
  noCopyConstructor = "%1 owns memory but has no copy constructor: add one (deep copy) or delete it (= delete), and the same for operator=.",
  noOperatorEq = "%1 owns memory but has no operator=: add one (deep copy) or delete it (= delete).",
  noDestructor = "%1 allocates memory but has no destructor to free it: add one, or hold the memory in a smart pointer.",
  copyCtorAndEqOperator = "Define the copy constructor and operator= together (rule of three), or delete both.",
  operatorEqVarError = "%1 isn't copied in operator=: assign it too.",
  operatorEqToSelf = "operator= doesn't handle self-assignment: check if (this == &other) first, or copy-and-swap.",
  virtualCallInConstructor = "%1 is virtual, but called from the constructor/destructor it runs the base version: call it after construction instead.",
  virtualDestructor = "%1 is deleted through a base pointer: give the base class a virtual destructor.",
  missingOverride = "%1 overrides a base function: add override, so the compiler catches signature mismatches.",
  useInitializationList = "Set %1 in the constructor's initializer list instead of assigning it in the body.",
  useStlAlgorithm = "This loop can be a standard algorithm (std::find_if, std::any_of, std::accumulate...), which says what it does.",
  postfixOperator = "Use ++x instead of x++ on %1: for iterators and classes the postfix form makes a copy.",
  stlcstr = "Pass the std::string itself instead of .c_str(): converting back makes a needless copy.",
  stlIfStrFind = "Use starts_with() (C++20) or compare(0, n, s) == 0 instead of find() == 0, which searches the whole string.",
  stlSize = "Use empty() instead of size() == 0.",
  containerSizeEmpty = "Use empty() instead of size() == 0.",
  invalidContainer = "%1 can be invalidated here (the container changes while you hold an iterator or reference into it): re-fetch it after the change.",
  invalidIterator1 = "The iterator is used after it's invalidated: re-fetch it after the container changes.",
  eraseDereference = "The iterator is used after erase(): use the iterator erase() returns.",
  danglingLifetime = "%1 points into something that's gone after this line: keep the object alive, or copy the data.",
  danglingTemporaryLifetime = "%1 points into a temporary that's gone at the end of the statement: store the temporary in a variable first.",
  returnDanglingLifetime = "This returns a pointer or reference to a local: return by value, or make the object outlive the call.",
  autoVariables = "A pointer to a local escapes the function: return by value, or allocate it so it outlives the call.",
  returnReference = "This returns a reference to a local: return by value instead.",
  returnTempReference = "This returns a reference to a temporary: return by value instead.",
  integerOverflow = "This can overflow: do the arithmetic in a wider type, or check the operands first.",
  signConversion = "A negative value turns into a huge unsigned one here: check it's >= 0 first, or keep both sides signed.",
  shiftTooManyBits = "The shift is wider than the type: use a wider type (1ULL << n) or check n.",
  zerodiv = "This can divide by zero: check the divisor first.",
  zerodivcond = "The divisor is compared with zero elsewhere, so it can be zero here: check it before dividing.",
  moduloofone = "x % 1 is always 0: this is probably meant to be another value.",
  comparePointers = "These pointers point into different objects, so comparing them is meaningless: compare indexes or the values.",
  assignmentInCondition = "This assigns in a condition: use == if you meant to compare, or add parentheses to show it's on purpose.",
  clarifyCalculation = "Add parentheses: the precedence here isn't what it looks like.",
  clarifyCondition = "Add parentheses: the precedence here isn't what it looks like.",
  badBitmaskCheck = "A bitmask check with | or a constant is always true: use & to test the bit.",
  memsetClass = "memset on a class with a vtable or non-trivial members breaks it: assign the members, or value-initialize ({}).",
  memsetClassFloat = "memset to 0 on floats is fine, but not to other bytes: assign the value instead.",
  sizeofwithsilentarraypointer = "sizeof on an array parameter gives the pointer's size: pass the length as a parameter.",
  sizeofDivisionMemfunc = "sizeof here doesn't give the number of elements: pass the count explicitly.",
  pointerSize = "sizeof of a pointer, not what it points to: use sizeof(*ptr) or the element type.",
  selfAssignment = "%1 is assigned to itself: it's probably meant to be another variable.",
  missingReturn = "Not every path returns a value: add a return at the end.",
  unsafeClassCanLeak = "%1 owns memory with no destructor or rule-of-three: hold it in a smart pointer or add the missing members.",
  catchExceptionByValue = "Catch the exception by const reference (catch (const T& e)) to avoid slicing and a copy.",
  throwInNoexceptFunction = "This throws in a noexcept function, which terminates the program: catch it, or drop noexcept.",
  exceptThrowInDestructor = "Throwing from a destructor terminates the program: catch it inside the destructor.",
  CastIntegerToAddressAtReturn = "Returning an integer as a pointer loses bits on 64-bit: return the right type.",
  AssignmentAddressToInteger = "Storing a pointer in an int loses bits on 64-bit: use uintptr_t or keep it a pointer.",
  cstyleCast = "Use static_cast / reinterpret_cast (whichever applies) instead of a C cast, so the intent is clear.",
  syntaxError = "cppcheck couldn't parse this: usually a missing macro definition or include path, not a real error.",
  unknownMacro = "cppcheck doesn't know this macro: define it for cppcheck (-D) or ignore the finding.",
  missingInclude = "cppcheck can't find this header: it's about cppcheck's include paths, not your code.",
}

-- The quoted names in a cppcheck message, in order: 'p', "x".
local function names(msg)
  local out = {}
  for q in msg:gmatch("'([^']+)'") do out[#out + 1] = q end
  -- Or the name at the end: "Null pointer dereference: p".
  if #out == 0 then out[1] = msg:match(":%s*([%w_][%w_%.%->]*)%s*$") end
  return out
end

function M.for_finding(id, msg)
  local text = id and ADVICE[id]
  if not text then return nil end
  local n = names(msg or "")
  local first = n[1] and ("'" .. n[1] .. "'") or "the variable"
  local second = n[2] and ("'" .. n[2] .. "'") or first
  text = text:gsub("%%1", (first:gsub("%%", "%%%%"))):gsub("%%2", (second:gsub("%%", "%%%%")))
  return (text:gsub("%%%%", "%%"))
end

return M
