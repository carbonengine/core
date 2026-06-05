// Copyright © 2025 CCP ehf.

#include <stack>
#include <gtest/gtest.h>
#include <CCPLog.h>


struct LogEntry
{
	CCP::LogType type;
	unsigned long userData;
	char message[1024];
};

std::stack<LogEntry> logstack;


void LogTracker( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
{
	LogEntry entry;
	strncpy(entry.message, message, std::extent_v<decltype(entry.message)>);
	entry.message[std::extent_v<decltype(entry.message)> - 1] = '\0';
	entry.type = type;
	entry.userData = userData;
	logstack.push(entry);
}

class CCPLog : public ::testing::Test 
{
	protected:
		virtual void TearDown()
		{
			while(!logstack.empty())
			{
				logstack.pop();
			}
			
			CCP::UnregisterLogEcho( LogTracker );
		}
};

TEST_F ( CCPLog, TestCanLogSingleLine )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	constexpr const char *s = "One line";
	CCP_LOG(s);

	EXPECT_EQ( 1, logstack.size() );
	LogEntry entry = logstack.top();
}

TEST_F ( CCPLog, TestDefaultLogLevelIsInfo )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	constexpr const char *s = "One line";
	CCP_LOG(s);

	EXPECT_EQ( 1, logstack.size() );
	LogEntry entry = logstack.top();
	EXPECT_EQ( CCP::LOGTYPE_INFO, entry.type );
}

TEST_F ( CCPLog, TestShortMessage )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	CCP_LOG("One line");

	EXPECT_EQ( 1, logstack.size() );
	LogEntry entry = logstack.top();
	EXPECT_STREQ( "One line", entry.message );
}

TEST_F ( CCPLog, WarningsShowUpWhenLogTypeIsInfo )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	CCP_LOGWARN("Warning");

	EXPECT_EQ( 1, logstack.size() );
}

TEST_F ( CCPLog, InfoLevelMessageIsHiddenWhenLogTypeIsWarn )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_WARN, true );
	CCP_LOG("info");

	EXPECT_EQ( 0, logstack.size() );
}

TEST_F ( CCPLog, HasNoCallbackHandlerByDefault )
{
	bool expected = false;
	bool actual = CCP::IsLogging();

	EXPECT_EQ( expected, actual );
}

TEST_F ( CCPLog, HasCallbackHandlerAfterRegisteringOne )
{
	CCP::RegisterLogEcho( CCP::LogToDebugger, CCP::LOGTYPE_INFO );

	bool expected = true;
	bool actual = CCP::IsLogging();
	CCP::UnregisterLogEcho( CCP::LogToDebugger );
	EXPECT_EQ( expected, actual );
}

TEST_F ( CCPLog, CanUnregisterCallbackHandler )
{
	CCP::RegisterLogEcho( CCP::LogToDebugger, CCP::LOGTYPE_INFO );
	CCP::UnregisterLogEcho( CCP::LogToDebugger );

	bool expected = false;
	bool actual = CCP::IsLogging();
	EXPECT_EQ( expected, actual );
}

TEST_F ( CCPLog, GetLastErrorMessageReturnsLastError )
{
	constexpr const char* expected = "Something has gone horribly wrong.";
	CCP_LOGERR( expected );
	const char* actual = CCP::GetLastErrorMessage();
	EXPECT_STREQ( expected, actual );
}

TEST_F ( CCPLog, ThrowLastErrorThrowsAnError )
{
	constexpr const char* error_str = "Something has gone horribly wrong.";
	CCP_LOGERR( error_str );
	EXPECT_ANY_THROW( {CCP::ThrowLastError();} );	
}

TEST_F ( CCPLog, LogEnormousLine )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	CCP::SetLogMainThreadId();
	constexpr const char* enormous_line =
		"Testing a really long log line. The log server can't handle log lines\n"
		"that are longer than 253 characters, so we need a test case for it.\n"
		"Somewhere along the line we need to split the log message into several\n"
		"lines, and this is done on the newline character.\n"
		"If individual lines are still too long they will be truncated.\n"
		"Let's try it:\n"
		"A123456789B123456789C123456789D123456789E123456789F123456789G123456789H123456789I123456789J123456789K123456789L123456789M123456789N123456789O123456789P123456789Q123456789R123456789S123456789T123456789V123456789W123456789X123456789Y123456789Z123456789a123456789c123456789d123456789e123456789\n"
		"And we're back\n";
		
	CCP_LOG( enormous_line );
	LogEntry entry = logstack.top();
	EXPECT_STREQ( enormous_line, entry.message );
}

TEST_F( CCPLog, FormatSpecifiers )
{
	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	CCP_LOG( "Let's try strings with format specifiers in them - %%s %%d\n" );
	LogEntry entry = logstack.top();
	EXPECT_STREQ( "Let's try strings with format specifiers in them - %s %d\n", entry.message );
}

TEST_F( CCPLog, SetLogtypePrivilegeFlag )
{
	EXPECT_FALSE( CCP::SetLogtypeInfoIsPrivileged( true ) );
	EXPECT_TRUE( CCP::SetLogtypeInfoIsPrivileged( false ) );
	EXPECT_FALSE( CCP::SetLogtypeInfoIsPrivileged( false ) );

	EXPECT_FALSE( CCP::SetLogtypeNoticeIsPrivileged( true ) );
	EXPECT_TRUE( CCP::SetLogtypeNoticeIsPrivileged( false ) );
	EXPECT_FALSE( CCP::SetLogtypeNoticeIsPrivileged( false ) );

	EXPECT_FALSE( CCP::SetLogtypeWarnIsPrivileged( true ) );
	EXPECT_TRUE( CCP::SetLogtypeWarnIsPrivileged( false ) );
	EXPECT_FALSE( CCP::SetLogtypeWarnIsPrivileged( false ) );

	EXPECT_FALSE( CCP::SetLogtypeErrIsPrivileged( true ) );
	EXPECT_TRUE( CCP::SetLogtypeErrIsPrivileged( false ) );
	EXPECT_FALSE( CCP::SetLogtypeErrIsPrivileged( false ) );
}

TEST_F( CCPLog, MemorySafe )
{
	const char* veryLongText = R"LOREMIPSUM(
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus fringilla consequat turpis. Mauris eu finibus nunc, at placerat diam. Integer ac orci laoreet, pulvinar orci vitae, consequat orci. Praesent consequat eget ipsum nec vulputate. Nam bibendum turpis sed ipsum ullamcorper, eget malesuada ipsum imperdiet. Interdum et malesuada fames ac ante ipsum primis in faucibus. Sed at lacus fringilla elit congue vestibulum. Maecenas in lobortis lectus. Maecenas vel viverra nunc. Nullam enim neque, sagittis non dolor sit amet, condimentum efficitur nisl. Donec interdum pulvinar leo, nec gravida nunc dignissim eget. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam quis ante eros. Integer finibus, magna nec elementum consequat, magna sem blandit mi, auctor dictum tortor tellus a nulla.
Nulla facilisi. Maecenas id tellus nulla. Aenean finibus condimentum mauris ac rutrum. Aliquam erat volutpat. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque ut urna sit amet lorem vulputate semper. Maecenas et eros arcu. Nunc lorem nisl, euismod ut eleifend nec, posuere sed ligula. Sed in dui accumsan, placerat augue vel, fringilla eros. Etiam sed porta dolor. Aliquam placerat dictum nisi, ut placerat velit dignissim nec.
Ut mattis ac enim vitae dignissim. Duis interdum malesuada arcu eu suscipit. Pellentesque sapien lectus, dapibus a feugiat et, dignissim ac nisl. Cras euismod sit amet odio eu congue. Cras vel blandit tortor, at feugiat libero. Phasellus posuere dictum sagittis. Donec eget orci a ex condimentum tempus.
Aliquam eget quam eros. Quisque vitae turpis eu ligula placerat porta quis ac arcu. Sed volutpat sed massa a ullamcorper. Cras rutrum purus sit amet justo tempor eleifend. Nunc ut dolor tempus, pharetra lacus in, pharetra neque. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Interdum et malesuada fames ac ante ipsum primis in faucibus. Donec lobortis id nulla eu feugiat. In pretium pulvinar felis, vel iaculis est fermentum et. Phasellus dignissim turpis sit amet leo mattis laoreet. Vivamus hendrerit elementum dolor, quis vehicula dolor lobortis ut. Mauris ultricies felis eu tempor sagittis. Nunc ut augue libero. Suspendisse porttitor dapibus nunc, dictum mattis erat commodo nec. Sed sit amet tortor arcu.
Integer iaculis tellus eget vulputate condimentum. Donec a laoreet nunc. Nullam in egestas orci. Pellentesque ullamcorper, magna at placerat interdum, dolor erat pretium elit, sit amet accumsan neque nulla nec turpis. Vivamus finibus, lacus sed fringilla blandit, dolor nunc dictum orci, id fringilla nibh dolor in ligula. Sed placerat viverra dolor, vel rutrum dui vulputate a. Aliquam aliquam augue in feugiat tincidunt. Duis ut leo eu sapien euismod consequat. Aenean sit amet metus odio. Cras rhoncus ligula a dui mollis, nec bibendum odio hendrerit. Sed ac elit eget orci maximus faucibus quis sed lectus. Nam egestas purus eu iaculis ullamcorper. Quisque ullamcorper, lacus eget tincidunt elementum, lectus massa ullamcorper arcu, ac ultrices mauris risus quis lacus. Mauris elit orci, interdum vel massa laoreet, cursus fringilla justo. Aliquam vehicula enim lacus, sit amet ullamcorper nunc porta in.
Proin lacinia hendrerit pellentesque. In vel lacus nulla. Phasellus vulputate velit ac scelerisque laoreet. Nunc id libero ligula. Nunc elementum condimentum augue ut auctor. Etiam felis dolor, facilisis eget libero quis, interdum viverra nisi. Donec vel est vulputate, ullamcorper risus in, volutpat justo. Cras odio mauris, cursus quis nisi quis, molestie tincidunt ante. In vulputate elit et ultrices tristique. Sed ut sem pellentesque, iaculis ipsum sit amet, accumsan nisl. Suspendisse dignissim metus in est ornare, eget finibus dolor finibus. Nulla nec suscipit libero. Mauris eu ante nunc. Interdum et malesuada fames ac ante ipsum primis in faucibus.
Aenean dictum nibh nec nulla pulvinar pulvinar. Integer lacinia, purus at consequat vestibulum, lectus ante condimentum enim, ac tincidunt tellus sem et metus. Morbi eu tristique velit. Donec eget consequat nisl. Pellentesque cursus enim nulla, ac rhoncus nisi efficitur quis. Maecenas sit amet malesuada dolor, vitae mollis eros. Fusce quis tristique elit. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Quisque mattis dui quis cursus ultricies. Aliquam feugiat ullamcorper tortor eget tincidunt. Vivamus tellus diam, hendrerit vel lorem vel, pulvinar sodales diam.
Maecenas scelerisque, est quis accumsan vestibulum, felis ligula venenatis tortor, ac finibus ante tellus ac orci. Vivamus pharetra condimentum turpis vel sodales. Phasellus finibus, nisl in pulvinar placerat, risus dui auctor nisl, quis rutrum nisl nulla nec enim. Cras mattis aliquet arcu, a sagittis eros sollicitudin non. Nulla porta ultrices nisl in interdum. Mauris eget dolor ac nisl aliquam feugiat sed a nisl. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Sed finibus sodales tempor.
Sed sit amet ligula posuere nisl sollicitudin tempus quis sed purus. Duis non mattis metus. Sed non neque a nisl egestas tincidunt. Nulla sollicitudin diam ut nunc pulvinar, ut lacinia lorem tincidunt. Aliquam tellus libero, pellentesque vel euismod vitae, mollis a felis. Sed pulvinar justo sed nulla ornare, in porttitor justo sodales. Fusce porta viverra gravida. Pellentesque diam mauris, mattis nec sapien et, commodo varius orci. Curabitur vitae odio lacus. Nunc feugiat turpis eu tincidunt sollicitudin. Donec dictum nunc id lectus facilisis, ut venenatis erat lacinia. Quisque consequat vulputate nunc, et mattis sapien vehicula in.
Ut orci ipsum, iaculis at arcu vehicula, tempor ullamcorper ante. Vivamus mauris risus, pellentesque vitae mattis vitae, condimentum vel massa. In hac habitasse platea dictumst. Phasellus eget tempus eros. Quisque porttitor, velit sed imperdiet vestibulum, lectus metus dictum mauris, nec viverra ligula libero non ipsum. Pellentesque vehicula lectus vel turpis commodo, sed dignissim justo auctor. Sed nec pulvinar diam. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam tempor dignissim massa sit amet blandit. Pellentesque rhoncus pellentesque odio vel aliquam. Suspendisse eu libero tincidunt, auctor enim eget, bibendum nibh. Maecenas a condimentum leo. Fusce feugiat fringilla iaculis. Aliquam ut maximus purus, id finibus nunc. Sed facilisis fringilla arcu ut malesuada. Proin quis lectus a elit pulvinar bibendum vel in mi.
Praesent vitae sagittis nisi. Duis mollis odio non consequat vehicula. Ut id erat pellentesque, eleifend enim vel, imperdiet tellus. Cras id nisi quis elit condimentum finibus. Phasellus quis mi aliquam, eleifend erat in, dictum risus. Quisque sit amet egestas nisl. Phasellus eu tincidunt quam. Duis vulputate mauris feugiat, egestas massa quis, cursus nibh. Donec laoreet ex nulla, non viverra lectus semper eget. Nullam vestibulum, ante ultrices viverra lobortis, nulla eros feugiat massa, eget maximus mi ex id turpis. Sed aliquam urna eu nisl dictum, a malesuada lacus varius. Vestibulum bibendum ante ligula, lacinia tincidunt sapien blandit a. Integer commodo ultrices velit, sed sollicitudin odio ultrices non. Proin sapien nisi, ornare pretium sollicitudin eu, fringilla sit amet ipsum.
Vestibulum semper porta tempus. Pellentesque in sagittis tortor. Vestibulum ullamcorper arcu et ante tempor dictum. Fusce enim nisl, porttitor a arcu nec, rutrum porttitor libero. Integer condimentum tincidunt orci. Duis consequat magna mauris, sit amet pulvinar quam consequat at. Suspendisse potenti.
Aenean vel orci sit amet magna vulputate lacinia non sed nisi. Quisque in augue sit amet elit dapibus consectetur non sit amet sem. Cras sed dapibus diam, eget dapibus arcu. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Duis nec elit mi. Nullam tincidunt egestas metus eget malesuada. Sed vitae nulla sed quam tincidunt auctor. Cras gravida lectus libero, eget rhoncus ligula varius ac. Duis eget felis at leo volutpat porttitor nec nec ex. Vivamus ultricies nec libero ac malesuada. Praesent in tortor ligula. Praesent scelerisque arcu lectus. Suspendisse tincidunt sapien quis urna bibendum, sed laoreet massa auctor. Phasellus quis tincidunt dolor, vitae vulputate leo. Sed vitae nisl libero.
Cras tempus purus ac lorem vulputate, feugiat vulputate nibh lacinia. Cras suscipit rhoncus lacus sit amet tincidunt. Maecenas at porta nisl, eget faucibus justo. Aenean commodo fermentum odio non egestas. Morbi sodales eleifend risus et accumsan. Vivamus efficitur ullamcorper nulla, a condimentum nunc vestibulum at. Nulla ultrices arcu eget risus gravida interdum id vel tellus.
Ut non porttitor elit. Quisque scelerisque dolor elit, et viverra leo elementum ac. Fusce ornare a ipsum non aliquam. Nam eget ipsum volutpat, fermentum tellus sit amet, ultricies lectus. Nulla suscipit porta turpis sit amet tempor. Pellentesque tincidunt turpis lorem, quis interdum sem viverra et. Vestibulum eu justo turpis. Morbi nec fringilla neque. Sed lobortis enim urna, vestibulum molestie nulla suscipit sed. Integer ultrices orci tellus. Proin rutrum efficitur enim ac accumsan. Curabitur sollicitudin rhoncus ante, a consequat nibh euismod vitae. Mauris sit amet sollicitudin diam, sed blandit nisl. Maecenas pellentesque, ante quis fringilla elementum, eros massa lacinia orci, sed consectetur purus justo quis ligula. Proin nec rutrum erat, a sodales massa.
In eget lacus nunc. Aenean at massa lobortis, tempor lacus id, fermentum eros. Maecenas vehicula est dolor, sed aliquet purus feugiat ultrices. Aliquam nunc augue, dapibus ac eros et, tristique rutrum nulla. Mauris felis odio, sodales nec congue nec, elementum eu justo. Maecenas interdum non lectus vitae sagittis. Mauris quis augue a dolor pulvinar mollis et eget erat. Aenean vel metus tortor. Nullam mollis et odio et faucibus.
Cras fringilla, elit et commodo efficitur, orci ante venenatis diam, eu tempor enim ante et augue. Proin venenatis, turpis in fermentum blandit, urna justo vulputate metus, sed egestas purus nibh vitae leo. Sed cursus diam eu nibh bibendum facilisis. Phasellus rhoncus, sapien vel fringilla vehicula, eros ligula ornare magna, nec rhoncus nisi sapien id eros. Proin sed quam lobortis nunc tristique tempor. Nunc porta libero eu purus tincidunt, in eleifend ante imperdiet. Sed condimentum tempor nibh ut tincidunt.
Aenean et nunc vitae arcu facilisis aliquam ut ac nibh. Praesent finibus dolor in congue aliquam. Vestibulum dapibus tristique ligula, vitae rhoncus augue venenatis et. Quisque in porttitor justo, eu elementum lectus. Quisque ex turpis, fermentum ut tempor ut, lacinia quis arcu. Ut vulputate tellus id sapien mollis, non tristique massa vehicula. Quisque sit amet mollis lorem. Nam imperdiet rhoncus tortor, quis venenatis est egestas vitae. Nunc sed risus ac elit pellentesque finibus. Sed venenatis, ligula ut lobortis feugiat, ex mauris auctor turpis, eu cursus dolor nisl ut sapien. Phasellus rutrum sagittis purus non molestie.
Curabitur sodales erat purus, et laoreet dolor pretium vel. Morbi quis nunc at lacus cursus interdum sit amet pharetra ex. Integer mollis posuere odio, non sodales lectus scelerisque et. Donec eget elit at turpis fermentum tempor. Phasellus iaculis dignissim nibh, non rhoncus arcu tempor pulvinar. Maecenas ultricies elit ut ante interdum fringilla imperdiet vitae libero. Vestibulum quis magna placerat, interdum risus et, porta nisi. Mauris vitae lectus a orci porttitor pharetra ac eget purus. Pellentesque convallis gravida purus, id euismod justo faucibus nec. Interdum et malesuada fames ac ante ipsum primis in faucibus. Pellentesque ligula dui, vestibulum eu elementum eget, accumsan in lectus. Etiam consequat scelerisque mattis. Donec eu interdum turpis. Morbi erat leo, hendrerit tempor orci eu, cursus maximus nisi. Maecenas tempor nulla libero, vitae venenatis tellus pretium eu. Nam euismod, elit id congue mattis, orci nisi aliquam libero, quis malesuada metus augue at massa.
In elementum ultrices ipsum vel vehicula. Nam mattis eu dolor a dignissim. Duis quis blandit elit. Pellentesque quis maximus turpis. Integer eu pellentesque lectus, vitae fermentum lectus. Duis hendrerit mauris pellentesque, congue ligula nec, ultricies odio. Aliquam vitae aliquet est.
Donec luctus porttitor elit, ac auctor massa vulputate sit amet. Pellentesque mattis dictum enim, sed commodo tellus iaculis eget. Pellentesque a libero sapien. Maecenas sed elit pulvinar, ultrices neque ut, suscipit ipsum. In hac habitasse platea dictumst. Ut id ex eu orci imperdiet varius. Nam tincidunt enim sed metus lacinia, eget sagittis ipsum dignissim. Praesent vehicula ultrices elit, ac efficitur nisl tempus varius. Sed eu vehicula tortor, vel tristique urna. Nunc vel velit nulla. Curabitur euismod urna sapien, at ultrices ante placerat eget.
Vestibulum dignissim lorem non dolor posuere, nec finibus odio luctus. Integer finibus, magna non efficitur pellentesque, est turpis ultrices dolor, ac finibus erat metus eget ipsum. In fermentum nibh lacus, a malesuada metus aliquet sit amet. Aenean eu risus purus. Morbi consequat lectus mi, non sollicitudin urna maximus et. Ut non metus hendrerit, luctus nisl nec, euismod est. Fusce at dui ut purus sagittis convallis. Donec posuere, est id placerat gravida, orci leo sodales tellus, vitae fringilla nisl dolor sit amet justo.
Nullam vulputate neque et lobortis sagittis. Cras ornare ultricies augue quis iaculis. Suspendisse porttitor sapien dolor, at accumsan mi molestie sed. Phasellus non metus aliquam, viverra lacus nec, feugiat nisi. Maecenas ornare ligula ut interdum efficitur. Maecenas ac facilisis odio. Duis eu commodo nisi, non semper urna. Nulla vehicula ultrices lacus, eget ultricies ligula vehicula eu. Sed ullamcorper felis ut faucibus tristique. Vestibulum vitae aliquam sapien, in accumsan tellus. Etiam nec lobortis libero.
Vestibulum luctus lacinia leo, vitae luctus ipsum efficitur quis. Sed a placerat nisi. Proin consectetur, ipsum et porta tincidunt, velit risus tristique lorem, sit amet fringilla massa velit non felis. Integer risus risus, pellentesque nec leo et, congue pellentesque ante. Praesent vehicula dolor sem, vel gravida sapien tincidunt sit amet. Nam arcu nisi, suscipit ac varius at, consequat sed lorem. Cras viverra fringilla nisi a cursus.
Suspendisse gravida lectus nec lectus cursus, et ultrices nulla aliquam. Curabitur arcu est, euismod sed lorem et, gravida bibendum est. Vestibulum ac nisi a enim bibendum pretium. Sed tortor ipsum, gravida fringilla felis eget, mattis commodo est. Nullam gravida, nisi in pellentesque tristique, justo leo elementum augue, et tincidunt tellus lacus at orci. Morbi non tincidunt enim. Morbi ex nisi, finibus ut enim fermentum, interdum pretium nulla. Ut sem ante, viverra quis lacus tincidunt, finibus vehicula lectus. Proin id suscipit ligula.
Proin a vestibulum leo. Nam sollicitudin iaculis tortor, nec semper quam scelerisque at. Maecenas posuere dapibus libero in mollis. Curabitur aliquam luctus arcu ac tristique. Sed hendrerit, augue ut scelerisque bibendum, urna odio mattis lectus, sit amet convallis nisl ex id ante. Mauris id feugiat nisi. Maecenas et risus rutrum metus ultricies ullamcorper quis quis ante. Aliquam bibendum pretium metus, in finibus ante scelerisque quis.
In hac habitasse platea dictumst. Mauris malesuada quam ex, eget lacinia mauris gravida sit amet. Fusce malesuada lorem nec nisl tristique, et efficitur diam hendrerit. Phasellus tempus nunc sit amet bibendum posuere. Nullam lorem quam, bibendum id elementum vel, mollis a ipsum. Vestibulum iaculis euismod finibus. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
Integer commodo lacus cursus dictum imperdiet. Vestibulum tempus eleifend dui at sodales. Nunc ac odio eleifend, sodales arcu quis, elementum ex. Nam at egestas augue. Ut elementum enim erat, quis feugiat eros tempor nec. Aenean eu consequat arcu. Proin in est nec orci cursus iaculis nec ultrices dolor. Pellentesque est libero, molestie id ullamcorper nec, rhoncus vitae tellus. Donec dictum, est in bibendum condimentum, elit odio sollicitudin mi, sit amet sagittis mi nunc nec metus. Aenean tempus urna vitae ante sagittis egestas. Fusce ac aliquet lorem.
)LOREMIPSUM";

	CCP::RegisterLogEcho( LogTracker, CCP::LOGTYPE_INFO, true );
	for ( int i = 0; i < 1000; ++i )
	{
		CCP_LOG( "%s%s%s%s%s%s", veryLongText, veryLongText, veryLongText, veryLongText, veryLongText, veryLongText );
	}
}
