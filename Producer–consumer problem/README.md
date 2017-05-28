Solution of Producer-consumer problem where:<br>
-producer produce elements in FIFO-buffer<br>
-3 readers reads them<br>
-one reader can't read the same element<br>
-element can't be read more then by two readers<br>
-consumer erase element after it was read at least by one reader<br> 
-if it was read by two readers consumer get this element and the next after it<br>
