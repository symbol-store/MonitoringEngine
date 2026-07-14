# Usage: cmake -DINPUT=<path> -DOUTPUT=<path> -DSYMBOL=<name> [-DBINARY=ON|-DCOMPRESS=ON] -P embed_file.cmake
# Text    : extern const char <SYMBOL>[] = R"---( <contents> )---";
# Binary  : extern const unsigned char <SYMBOL>[] = { <bytes> };  + <SYMBOL>_len
# Compress: gzip-compressed byte array + <SYMBOL>_len (compressed) + <SYMBOL>_rawlen (original)
if(COMPRESS)
  set(_gz "${OUTPUT}.gz")
  file(ARCHIVE_CREATE OUTPUT "${_gz}" PATHS "${INPUT}" FORMAT raw COMPRESSION GZip)
  file(SIZE "${INPUT}" _rawlen)
  file(READ "${_gz}" _hex HEX)
  string(REGEX REPLACE "(..)" "0x\\1," _bytes "${_hex}")
  file(WRITE "${OUTPUT}"
       "extern const unsigned char ${SYMBOL}[] = {${_bytes}};\n"
       "extern const unsigned long ${SYMBOL}_len = sizeof(${SYMBOL});\n"
       "extern const unsigned long ${SYMBOL}_rawlen = ${_rawlen};\n")
elseif(BINARY)
  file(READ "${INPUT}" _hex HEX)
  string(REGEX REPLACE "(..)" "0x\\1," _bytes "${_hex}")
  file(WRITE "${OUTPUT}"
       "extern const unsigned char ${SYMBOL}[] = {${_bytes}};\n"
       "extern const unsigned long ${SYMBOL}_len = sizeof(${SYMBOL});\n")
else()
  file(READ "${INPUT}" _content)
  file(WRITE "${OUTPUT}"
       "extern const char ${SYMBOL}[] = R\"---(${_content})---\";\n")
endif()
