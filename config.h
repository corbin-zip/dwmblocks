//Modify this file to change what commands output to your statusbar, and recompile using the make command.

//Maximum seconds a block command may run before dwmblocks kills it and moves
//on, keeping the block's previous text. Keep this comfortably longer than any
//timeout the block scripts use internally (e.g. curl --max-time) so their own
//graceful fallbacks fire first.
#define CMDTIMEOUT 15

static const Block blocks[] = {
    /*Icon*/ /*Command*/ /*Update Interval*/ /*Update Signal*/
    /* {"⌨", "sb-kbselect", 0, 30}, */
    {"", "cat /tmp/recordingicon 2>/dev/null", 0, 9},
    {"", "sb-ticker", 180, 27},
    {"", "sb-tasks", 10, 26},
    {"", "sb-firmware", 18000, 21},
    {"", "sb-caffeine", 180, 29},
    {"", "sb-dnd", 180, 30},
    /* {"", "sb-timer", 1, 7}, */
    {"", "sb-music", 0, 11},
    {"", "sb-mailbox", 180, 12},
    {"", "sb-news", 0, 6},
    {"", "sb-pacpackages", 0, 8},
	/* {"",	"sb-price xmr Monero 🔒 24",			9000,	24}, */
	/* {"",	"sb-price eth Ethereum 🍸 23",	9000,	23}, */
	/* {"",	"sb-price btc Bitcoin 💰 21",				9000,	21}, */
    /*{"",	"sb-torrent",	20,	7},*/
    /* {"",	"sb-memory",	10,	14}, */
    /* {"",	"sb-cpu",		10,	18}, */
    /* {"", "sb-doppler", 0, 25}, */
    /* {"", "sb-moonphase", 18000, 17}, */
    {"", "sb-forecast", 18000, 5},
    {"", "sb-claude", 1200, 20},
    {"", "sb-nettraf", 1, 16},
    {"", "sb-volume", 0, 10},
    {"", "sb-snd-output", 180, 18},
    {"", "sb-battery", 5, 3},
    {"", "sb-clock", 60, 1},
    {"", "sb-internet", 5, 4},
    {"", "sb-help-icon", 0, 15},
};

//Sets delimiter between status commands. NULL character ('\0') means no delimiter.
static char *delim = " ";

// Have dwmblocks automatically recompile and run when you edit this file in
// vim with the following line in your vimrc/init.vim:

// autocmd BufWritePost ~/.local/src/dwmblocks/config.h !cd ~/.local/src/dwmblocks/; sudo make install && { killall -q dwmblocks;setsid dwmblocks & }

