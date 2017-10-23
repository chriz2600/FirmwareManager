// for browsers that don't support key property
keyboardeventKeyPolyfill.polyfill();

function typed(finish_typing) {
    return function(term, message, delay, finish) {
        anim = true;
        var prompt = term.get_prompt();
        var c = 0;
        if (message.length > 0) {
            term.set_prompt('');
            var new_prompt = '';
            var interval = setInterval(function() {
                var chr = $.terminal.substring(message, c, c+1);
                new_prompt += chr;
                term.set_prompt(new_prompt);
                c++;
                if (c == length(message)) {
                    clearInterval(interval);
                    // execute in next interval
                    setTimeout(function() {
                        // swap command with prompt
                        finish_typing(term, message, prompt);
                        anim = false
                        finish && finish();
                    }, delay);
                }
                $('#term').get(0).scrollTop = $('#term').get(0).scrollHeight;
            }, delay);
        }
    };
}
function length(string) {
    string = $.terminal.strip(string);
    return $('<span>' + string + '</span>').text().length;
}
var typed_prompt = typed(function(term, message, prompt) {
    term.set_prompt(message + ' ');
});
var typed_message = typed(function(term, message, prompt) {
    term.echo(message)
    term.set_prompt(prompt);
});

var anim = false;
var waiting = false;
var scanlines = $('.scanlines');
var tv = $('.tv');
var term = $('#term').terminal(function(command, term) {
    var finish = false;

    if (command.match(/^\s*exit\s*$/)) {
        $('.tv').addClass('collapse');
        term.disable();
    } else if (command.match(/^\s*help\s*$/)) {
        help(true);
    } else if (command.match(/^\s*test\s*$/)) {
        waiting = true;
        var msg = "Please select file to upload...";
        term.set_prompt('');
        term.find('.cursor').hide();
        typed_message(term, msg, 1, function() {
            finish = true;
        });
        /*
        var args = {command: cmd};
        $.get('...', args, function(result) {
            (function wait() {
                if (finish) {
                    term.echo(result);
                } else {
                    setTimeout(wait, 500);
                }
            })();
        });*/
        setTimeout(function() {
            (function wait() {
                if (finish) {
                    term.echo("...");
                    term.set_prompt('dc-hdmi> ');
                    term.find('.cursor').show();
                    waiting = false;
                } else {
                    setTimeout(wait, 500);
                }
            })();
        }, 10000);

    } else if (command !== '') {
        term.error('unkown command, try:');
        help();
    } else {
        term.echo('');
    }
}, {
    name: 'dc-hdmi',
    onResize: set_size,
    exit: false,
    onInit: function(term) {
        set_size();
        typed_message(term, 'type [[b;#fff;]help] to get help', 75);
    },
    prompt: 'dc-hdmi> ',
    greetings: [
        '     ____                                           __ ',
        '    / __ \\ ___ _____ ___   ______ ____ ___   ____ / /_',
        '   / / / // _// _  // _ \\ /     // __// _ \\ /  _// __/',
        '  / /_/ // / / ___// // // / / // /_ / // /_\\ \\ / /__ ',
        ' /_____//_/ /____/ \\__\\_/_/ /_//___/ \\__\\_/____/\\___/',
        '                                   HDMI by chriz2600',
        ''
    ].join('\n'),
    keydown: function(e) {
        //disable keyboard when animating or waiting
        if (waiting || anim) {
            return false;
        }
    }
});
function set_size() {
    // for window heihgt of 170 it should be 2s
    var height = $(window).height();
    var time = (height * 2) / 170;
    scanlines[0].style.setProperty("--time", time);
    tv[0].style.setProperty("--width", $(window).width());
    tv[0].style.setProperty("--height", height);
    
}

function help(full) {
    var msg = "";
    if (full) {
        msg = " \n"
            + "To [[b;#fff;]flash] the firmware, you have 2 options:\n"
            + " \n"
            + "1) [[b;#fff;]select] a firmware from your harddisk\n"
            + "   and [[b;#fff;]upload] it to the staging area\n"
            + "2) [[b;#fff;]download] the latest firmware from dc.i74.de\n"
            + "   to the staging area\n"
            + " \n"
            + "After that, you are ready to [[b;#fff;]flash] the firmware\n"
            + "to the FPGA configuration memory. It's possible to re-flash\n"
            + "the firmware from the staging area at any time, because it's\n"
            + "stored in the flash of the WiFi chip.\n"
            + " \n";
    }
    msg += "[[b;#fff;]select]:   select file to upload\n";
    msg += "[[b;#fff;]upload]:   upload selected file\n";
    msg += "[[b;#fff;]download]: download latest flash file from dc.i74.de\n";
    msg += "[[b;#fff;]flash]:    flash uploaded file\n";
    typed_message(term, msg, 1);
}
