// Copyright © 2025 CCP ehf.

#include "gtest/gtest.h"
#include "StringConversions.h"

std::vector<std::pair<std::wstring, std::string>> validUnicodeStrings{
	{ L"The quick brown fox jumps over the lazy dog", u8"The quick brown fox jumps over the lazy dog" },
	{ L"Wolther spillede på xylofon.", u8"Wolther spillede på xylofon." },
	{ L"Zwölf Boxkämpfer jagten Eva quer über den Sylter Deich", u8"Zwölf Boxkämpfer jagten Eva quer über den Sylter Deich" },
	{ L"Γαζέες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο", u8"Γαζέες καὶ μυρτιὲς δὲν θὰ βρῶ πιὰ στὸ χρυσαφὶ ξέφωτο" },
	{ L"El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y frío, añoraba a su querido cachorro.", u8"El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y frío, añoraba a su querido cachorro." },
	{ L"Le cœur déçu mais l'âme plutôt naïve, Louÿs rêva de crapaüter en canoë au delà des îles, près du mälström où brûlent les novæ.", u8"Le cœur déçu mais l'âme plutôt naïve, Louÿs rêva de crapaüter en canoë au delà des îles, près du mälström où brûlent les novæ." },
	{ L"D'fhuascail Íosa, Úrmhac na hÓighe Beannaithe, pór Éava agus Ádhaimh", u8"D'fhuascail Íosa, Úrmhac na hÓighe Beannaithe, pór Éava agus Ádhaimh" },
	{ L"Árvíztűrő tükörfúrógép", u8"Árvíztűrő tükörfúrógép" },
	{ L"Sævör grét áðan því úlpan var ónýt", u8"Sævör grét áðan því úlpan var ónýt" },
	{ L"いろはにほへとちりぬる イロハニホヘト", u8"いろはにほへとちりぬる イロハニホヘト" },
	{ L"Pchnąć w tę łódź jeża lub ośm skrzyń fig", u8"Pchnąć w tę łódź jeża lub ośm skrzyń fig" },
	{ L"В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!", u8"В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!" },
	{ L"๏ เป็นมนุษย์สุดประเสริฐเลิศคุณค่า  กว่าบรรดาฝูงสัตว์เดรัจฉาน", u8"๏ เป็นมนุษย์สุดประเสริฐเลิศคุณค่า  กว่าบรรดาฝูงสัตว์เดรัจฉาน" },
	{ L"Pijamalı hasta, yağız şoföre çabucak güvendi.", u8"Pijamalı hasta, yağız şoföre çabucak güvendi." },
	{ L"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i),      ⎧⎡⎛┌─────┐⎞⎤⎫", u8"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i),      ⎧⎡⎛┌─────┐⎞⎤⎫" },
	{ L"ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn", u8"ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn" },
	{ L"((V⍳V)=⍳⍴V)/V←,V    ⌷←⍳→⍴∆∇⊃‾⍎⍕⌈", u8"((V⍳V)=⍳⍴V)/V←,V    ⌷←⍳→⍴∆∇⊃‾⍎⍕⌈" },
	{ L"STARGΛ̊TE SG-1, a = v̇ = r̈, a⃑ ⊥ b⃑", u8"STARGΛ̊TE SG-1, a = v̇ = r̈, a⃑ ⊥ b⃑" },
	{ L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო", u8"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო" },
	{ L"ሰማይ አይታረስ ንጉሥ አይከሰስ።", u8"ሰማይ አይታረስ ንጉሥ አይከሰስ።" },
	{ L"ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ", u8"ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ" },
	{ L"⡌⠁⠧⠑ ⠼⠁⠒  ⡍⠜⠇⠑⠹⠰⠎ ⡣⠕⠌", u8"⡌⠁⠧⠑ ⠼⠁⠒  ⡍⠜⠇⠑⠹⠰⠎ ⡣⠕⠌" },
	{ L"∀∂∈ℝ∧∪≡∞ ↑↗↨↻⇣ ┐┼╔╘░►☺♀ ﬁ�⑀₂ἠḂӥẄɐː⍎אԱა", u8"∀∂∈ℝ∧∪≡∞ ↑↗↨↻⇣ ┐┼╔╘░►☺♀ ﬁ�⑀₂ἠḂӥẄɐː⍎אԱა" },
	{ L"╠╡ ╳ ╞╣  ├╢   ╟┤  ├┼─┼─┼┤  ├╫─╂─╫┤  ┣┿╾┼╼┿┫  ┕┛┖┚     ┌┄┄┐ ╎ ┏┅┅┓ ┋ ▍ ╲╱╲╱╳╳╳", u8"╠╡ ╳ ╞╣  ├╢   ╟┤  ├┼─┼─┼┤  ├╫─╂─╫┤  ┣┿╾┼╼┿┫  ┕┛┖┚     ┌┄┄┐ ╎ ┏┅┅┓ ┋ ▍ ╲╱╲╱╳╳╳" },
	{ L"爱吃鱼的在我家", u8"爱吃鱼的在我家" },
	{ L"�", u8"�" }, // Surrogate pair in UTF-16
};

TEST( StringConversion, CanConvertUTF8ToUnicodeWStrings )
{
	for( auto [wstr, utf8] : validUnicodeStrings )
		EXPECT_EQ( UTF8ToWide( utf8 ), wstr );
}

TEST( StringConversion, CanConvertUnicodeWStringsToUTF8 )
{
	for( auto [wstr, utf8] : validUnicodeStrings )
		EXPECT_EQ( WideToUTF8( wstr ), utf8 );
}

TEST( StringConversion, ConvertingStringsTwiceLeavesThemUnchanged )
{
	for( auto [wstr, utf8] : validUnicodeStrings )
	{
		EXPECT_EQ( WideToUTF8( UTF8ToWide( utf8 ) ), utf8 );
		EXPECT_EQ( UTF8ToWide( WideToUTF8( wstr ) ), wstr );
	}
}
