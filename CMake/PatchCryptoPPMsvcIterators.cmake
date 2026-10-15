# Crypto++ 8.9 uses stdext checked iterators removed by current MSVC STL releases.
# The output range is sized explicitly and the deflate match range is bounded by
# scanEnd, so the equivalent standard pointer iterators are sufficient here.
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
        message(STATUS "Applied Crypto++ MSVC iterator compatibility patch: ${Description}")
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
