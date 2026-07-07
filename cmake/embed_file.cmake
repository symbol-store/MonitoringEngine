# Usage: cmake -DINPUT=<path> -DOUTPUT=<path> -DSYMBOL=<name> -P embed_file.cmake
# Emits: extern const char <SYMBOL>[] = R"---( <contents of INPUT> )---";
file(READ "${INPUT}" _content)
file(WRITE "${OUTPUT}"
     "extern const char ${SYMBOL}[] = R\"---(${_content})---\";\n")
