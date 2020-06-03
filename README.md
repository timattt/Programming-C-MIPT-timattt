# Введение
Сей репозиторий содержит мои проекты, написанные в первом семестре обучения на ФРТК.
Из них можно выделить несколько, представляющие наибольший интерес, краткая сводка по ним следует далее.

# Компилятор компиляторов
Пожалуй один из самых полезных проектов - алгоритм, позволяющий построить универсальный код для любой грамматики.
Полученный код сможет сгенерировать дерево грамматики для дальнейших с ним взаимодействий на усмотрение разработчика.

## Правила написания грамматики
- Каждая строка - нетерминал.
- Скобки есть только квадратные для описания терминальных символов.
- Если используем оператор | (или), то в каждой части оператора может быть только один нетерминал И БОЛЬШЕ НИЧЕГО,
приоритет идет слева.
- Можно задавать терминальные символы в квадратных скобках.
Внутри скобок выбирается строка, из заключенных в кавычки,
если в кавычках поставить два символа и между ними тире, то может быть выбран любой символ,
лежащий между этими двумя.
- После нетерминала можно ставить следующие операторы:
1. "*" - ноль или более.
2. "+" - один или более.
3. "?" - ноль или один.
- Нужно быть осторожным, когда есть два нетерминала с одинаковым началом.
- Результат работы парсера - дерево, где каждая вершина - нетерминал, ее сыновья - нетерминалы, которые вызываются данным нетерминалом.
В ней хранится значение, которое склеивается из значений сыновей. Если в определении вершины-нетерминала есть терминалы,
то они просто добавляются в значению данной вершины.

## Примеры
- __A ::=...;__ (Объявляем нетерминал А)
- __A ::=["abc"];__ (Нетерминал А определен как строка "abc")
- __A ::= B | C | D;__ (Если B подходит, то в A используется B, если нет, то проверяется C и т.д.)
- __A ::= ["abc" "b-z"];__ (Нетерминал А может быть одной из строк "abc" или одним символом из диапазона [b, z])
- __А ::= B*;__ (Пока нетерминал B присутствует он будет использован)
- __A ::= ["abcd"]E;__
__B ::= ["abcd"]C;__ (C, E - другие нетерминалы)
__D ::= A | B;__ (Вот в этом месте может возникнуть неопределенность)
- __A ::= C;__
__C ::= B["de"];__
__B ::= "abc";__
Результат работы для строки "abcde" (это дерево, в скобках указано значение, которое лежит в вершине)
A("abcde") -> C("abcde") -> B("abc")

# Интерпретатор
В конце первого семестра я написал язык программирования, но это был интерепретируемый язык, т.е. он выполнялся непосредственно в виде дерева, без перехода в асм, синтаксис примерно похож на C.

Все это располагается в папке tGrammarParser.
Для использования предполагается снача обработать строку с исходным кодом через функции обработки грамматики (файл tGrammarParser.h) (это все самодельное, когда я писал язык, компилятора компиляторов еще не было, так что код считается 'авто сгенерированным', поэтому доков в самом коде маловато),
запустить функции init и G (классический первый нетерминал)
Далее можно залезть в файл tInterpritator.h и запустить функции init, дабы подключить к собственно интерпретатору само дерево, и функцию run. И все будет работать.

# Транслятор
Еще есть собственный асемблер (tnasm) с переводом в машинный код собственного формата, который запускается эмулятором. Код аналогичен обычному nasm. Однако, разумеется, было бы интересно получать из этого асм полноценный exe файл. Поэтому есть две опции: использовать транслятор и переводить машинный код tnasm в машинный код windows (получать готовый PE файл). Или писать на специальной модификации tnasm, которая заточена специально под PE (помимо обычных команд процессора, есть еще сложные, который разворачиваются в несколько обычных, что существенно облегчает разработку (например команда пересылки данных из памяти в память)).

Чтобы всем этии воспользоваться, нужно использовать файл tProcessor/tProcessor.h и его функции tDisasm_texe, tCompile_texe, tCompile_exe, smart_translate_texe_to_exe. Думаю, что они делают интуитивно понятно.
