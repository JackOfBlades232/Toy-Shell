word.o: src/word.c src/word.h
word_list.o: src/word_list.c src/word_list.h src/word.h
line_tokenization.o: src/line_tokenization.c src/line_tokenization.h \
 src/word_list.h src/word.h
