# Compatibility adjustments for Crypto++ 8.9 with current MSVC toolchains.
# The output range is sized explicitly and the deflate match range is bounded by
# scanEnd, so standard pointer iterators replace removed stdext checked iterators.
function(PatchCryptoPPFile FilePath OldBlock NewBlock OldMarker Description)
    if(NOT EXISTS "${FilePath}")
        message(FATAL_ERROR "Crypto++ source file is missing: ${FilePath}")
    endif()

    file(READ "${FilePath}" Source)
    string(REPLACE "\r\n" "\n" Source "${Source}")
    string(FIND "${Source}" "${OldBlock}" OldPosition)
    if(OldPosition GREATER_EQUAL 0)
        string(REPLACE "${OldBlock}" "${NewBlock}" PatchedSource "${Source}")
        file(WRITE "${FilePath}" "${PatchedSource}")
        message(STATUS "Applied Crypto++ MSVC compatibility patch: ${Description}")
        return()
    endif()

    string(FIND "${Source}" "${NewBlock}" NewPosition)
    if(NewPosition GREATER_EQUAL 0)
        return()
    endif()

    string(FIND "${Source}" "${OldMarker}" MarkerPosition)
    if(MarkerPosition GREATER_EQUAL 0)
        message(FATAL_ERROR
            "Crypto++ ${Description} source no longer matches the expected 8.9 code")
    endif()
endfunction()

set(Tab "\t")
string(CONCAT IntegerOldBlock
    "#if (CRYPTOPP_MSC_VERSION >= 1500)\n"
    "${Tab}${Tab}std::reverse_copy(encodedInteger, encodedInteger+byteCount,\n"
    "${Tab}${Tab}${Tab}stdext::make_checked_array_iterator(block.begin(), block.size()));\n"
    "#else\n"
    "${Tab}${Tab}std::reverse_copy(encodedInteger, encodedInteger+byteCount, block.begin());\n"
    "#endif")
string(CONCAT IntegerNewBlock
    "${Tab}${Tab}std::reverse_copy(encodedInteger, encodedInteger+byteCount, block.begin());")
PatchCryptoPPFile(
    "${cryptopp_src_SOURCE_DIR}/integer.cpp"
    "${IntegerOldBlock}" "${IntegerNewBlock}"
    "stdext::make_checked_array_iterator" "Integer reverse-copy")

string(CONCAT DeflateOldBlock
    "#if CRYPTOPP_MSC_VERSION >= 1600\n"
    "${Tab}${Tab}${Tab}${Tab}(stdext::make_unchecked_array_iterator(scan)+3, stdext::make_unchecked_array_iterator(scanEnd), stdext::make_unchecked_array_iterator(match)+3).first - stdext::make_unchecked_array_iterator(scan));\n"
    "#else\n"
    "${Tab}${Tab}${Tab}${Tab}(scan+3, scanEnd, match+3).first - scan);\n"
    "#endif")
string(CONCAT DeflateNewBlock
    "${Tab}${Tab}${Tab}${Tab}(scan+3, scanEnd, match+3).first - scan);")
PatchCryptoPPFile(
    "${cryptopp_src_SOURCE_DIR}/zdeflate.cpp"
    "${DeflateOldBlock}" "${DeflateNewBlock}"
    "stdext::make_unchecked_array_iterator" "Deflate match")

# Crypto++ 8.9 routes MSVC x64 CPU queries through x64dll.asm, but this project
# intentionally builds the library with CRYPTOPP_DISABLE_ASM. Use MSVC's x64
# intrinsics for these two routines so the C++-only build has no MASM symbols.
string(CONCAT XGetBVOldBlock
    "#if defined(_M_X64) && defined(CRYPTOPP_MS_STYLE_INLINE_ASSEMBLY)\n"
    "\n"
    "\treturn XGETBV64(num);")
string(CONCAT XGetBVNewBlock
    "#if defined(_M_X64) && defined(CRYPTOPP_MS_STYLE_INLINE_ASSEMBLY)\n"
    "\n"
    "\treturn _xgetbv(num);")
PatchCryptoPPFile(
    "${cryptopp_src_SOURCE_DIR}/cpu.cpp"
    "${XGetBVOldBlock}" "${XGetBVNewBlock}"
    "return XGETBV64(num);" "MSVC x64 XGETBV intrinsic")

string(CONCAT CpuIdOldBlock
    "\tCPUID64(func, subfunc, output);\n"
    "\treturn true;")
string(CONCAT CpuIdNewBlock
    "\tint registers[4] = {};\n"
    "\t__cpuidex(registers, static_cast<int>(func), static_cast<int>(subfunc));\n"
    "\toutput[0] = static_cast<word32>(registers[0]);\n"
    "\toutput[1] = static_cast<word32>(registers[1]);\n"
    "\toutput[2] = static_cast<word32>(registers[2]);\n"
    "\toutput[3] = static_cast<word32>(registers[3]);\n"
    "\treturn true;")
PatchCryptoPPFile(
    "${cryptopp_src_SOURCE_DIR}/cpu.cpp"
    "${CpuIdOldBlock}" "${CpuIdNewBlock}"
    "CPUID64(func, subfunc, output);" "MSVC x64 CPUID intrinsic")
