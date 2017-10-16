# FreeTypeFont
<br>
SoFreeTypeFont.h<br>
SoFreeTypeFont.cpp<br>
这两个文件演示了加载磁盘上的FreeType文件，并生成指定的字形。<br>
<br>
SoFreeType.h<br>
SoFreeType.cpp<br>
这两个文件演示的是，先把FreeType文件加载到内存，再根据内存数据创建FreeType对象，并生成指定的字形。<br>
什么情况下会用到这两个文件？<br>
我在android平台下编程用到了这两个文件。我没办法让FreeType库自己去加载字体文件，<br>
我使用Android平台下的特定方法把字体文件加载到内存，然后根据内存数据创建FreeType对象。<br>
<br>
<br>
