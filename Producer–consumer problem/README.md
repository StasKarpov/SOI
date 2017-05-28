Solution of Producer-consumer problem where:
-producer produce elements in FIFO-buffer
-3 readers reads them
-one reader can't read the same element
-element can't be read more then by two readers
-consumer erase element after it was read at least by one reader 
-if it was read by two readers consumer get this element and the next after it
