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
                $('#term').scrollTop($('#term').prop('scrollHeight'));
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
function fileInputChange() {
    var files = $("#fileInput").get(0).files;
    if (files && files[0]) {
        var reader = new FileReader();
        reader.onload = function(event) {
            var spark = new SparkMD5.ArrayBuffer();
            var md5 = spark.append(event.target.result);
            lastMD5 = spark.end();
            endTransaction("Selected file:\n"
                + "Name: " + files[0].name + "\n" 
                + "Size: " + files[0].size + " Byte\n"
                + "MD5:  [[b;#fff;]" + lastMD5 + "]"
            );
        };
        reader.readAsArrayBuffer(files[0]);
    } else {
        endTransaction({ msg: "No file selected!", iserror: true });
    }
}

function startTransaction(msg, action) {
    finish = false;
    waiting = true;
    term.set_prompt('');
    term.find('.cursor').removeClass('blink');
    term.find('.cursor').hide();
    if ($.type(msg) === "object") {
        iserror = msg.iserror;
        msg = msg.msg || "";
    }
    if (msg) {
        typed_message(term, msg, 1, function() {
            finish = true;
        });
    } else {
        finish = true;
    }
    if (typeof(action) == "function") {
        action();
    }
}

function endTransaction(msg, iserror) {
    (function wait() {
        if (finish) {
            if (msg) {
                if ($.type(msg) === "object") {
                    iserror = msg.iserror || false;
                    msg = msg.msg || "";
                }

                if (iserror) {
                    term.error(msg);
                } else {
                    term.echo(msg);
                }
            }
            term.find('.cursor').show();
            term.find('.cursor').addClass('blink');
            term.set_prompt('dc-hdmi> ');
            $('#term').scrollTop($('#term').prop('scrollHeight'));
            waiting = false;
        } else {
            setTimeout(wait, 350);
        }
    })();
}

var lastMD5;
var progressSize = 40;
var anim = false;
var waiting = false;
var finish = false;
var scanlines = $('.scanlines');
var tv = $('.tv');
var term = $('#term').terminal(function(command, term) {
    if (command.match(/^\s*exit\s*$/)) {
        $('.tv').addClass('collapse');
        term.disable();
    } else if (command.match(/^\s*help\s*$/)) {
        help(true);
    } else if (command.match(/^\s*select\s*$/)) {
        startTransaction("Please select file to upload...", function() {
            $('#fileInput').click();
        });
        (function waitForDialog() {
            if (term.find('.cursor').hasClass('blink')) {
                setTimeout(function() {
                    fileInputChange();
                }, 150);
            } else {
                setTimeout(waitForDialog, 350);
            }
        })();
    } else if (command.match(/^\s*upload\s*$/)) {
        startTransaction(null, function() {
            uploadFile();
        });
    } else if (command.match(/^\s*download\s*$/)) {
        startTransaction(null, function() {
            downloadFile();
        });
    } else if (command.match(/^\s*file\s*$/)) {
        startTransaction(null, function() {
            fileInputChange();
        });
    } else if (command.match(/^\s*test\s*$/)) {
        startTransaction("Test");
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
            endTransaction("...");
        }, 1000);

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
        typed_message(term, 'Type [[b;#fff;]help] to get help!\n', 36);
    },
    prompt: 'dc-hdmi> ',
    greetings: [
        '    ____                                          __ ',
        '   / __ \\ ___ _____ ___   ______ ____ ___   ____ / /_',
        '  / / / // _// _  // _ \\ /     // __// _ \\ /  _// __/',
        ' / /_/ // / / ___// // // / / // /_ / // /_\\ \\ / /__ ',
        '/_____//_/ /____/ \\__\\_/_/ /_//___/ \\__\\_/____/\\___/',
        '                              __  __ __   ______ __',
        '                             / /_/ //  \\ /     // /',
        '       by chriz2600         / __  // / // / / // /',
        '                           /_/ /_//___//_/ /_//_/',
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

function progress(percent, width) {
    var size = Math.round(width*percent/100);
    var left = '', taken = '', i;
    for (i=size; i--;) {
        taken += '=';
    }
    if (taken.length > 0 && percent < 100) {
        taken = taken.replace(/=$/, '>');
    }
    for (i=width-size; i--;) {
        left += ' ';
    }
    return '[' + taken + left + '] ' + percent + '%';
}

function uploadFile() {
    var file = $("#fileInput").get(0).files[0];
    if (!file) {
        endTransaction("No file selected!", true);
        return;
    }
    var formData = new FormData();
    var client = new XMLHttpRequest();

    formData.append("file", file);

    client.onerror = function(e) {
        endTransaction("Error during upload!", true);
    };
 
    client.onload = function(e) {
        if (client.status >= 200 && client.status < 400) {
            $.ajax("/firmware.rbf.md5").done(function (data) {
                endTransaction(
                    progress(100, progressSize) + ' [[b;green;]OK]\n'
                    + "MD5 Check: " 
                    + (lastMD5 == data 
                       ? "[[b;green;]OK]" 
                       : "[[b;red;]MD5 mismatch: " + data + "]")
                    + (lastMD5 == data ? "" : "\nPlease try to re-upload file.")
                );
            }).fail(function() {
                endTransaction('Error reading checksum', true);    
            });
        } else {
            endTransaction('Error uploading file: ' + client.status, true);
        }
    };
 
    client.upload.onprogress = function(e) {
        var p = Math.round(100 / e.total * e.loaded);
        term.set_prompt(progress(p - 1, progressSize));
    };
    
    client.onabort = function(e) {
        endTransaction("Abort!", true);
    };

    term.set_prompt(progress(0, progressSize));

    client.open("POST", "/upload?size=" + file.size);
    client.send(formData);
}

function downloadFile() {
    startSpinner(term, spinners["shark"]);

    $.ajax("/download").done(function (data) {
        $.ajax("/firmware.rbf.md5").done(function (data) {
            var calcMd5 = $.trim(data);
            $.ajax("/firmware.rbf.orig.md5").done(function (data) {
                var origMd5 = $.trim(data);
                endTransaction(
                    'Download: [[b;green;]OK]\n'
                    + "MD5 Check: " 
                    + (calcMd5 == origMd5 
                        ? "[[b;green;]OK]" 
                        : "[[b;red;]MD5 mismatch: " + data + "]")
                    + (calcMd5 == origMd5 ? "" : "\nPlease try to re-download file.")
                );
            }).fail(function() {
                endTransaction('Error reading original checksum', true);    
            });
        }).fail(function() {
            endTransaction('Error reading checksum', true);    
        });
    }).fail(function() {
        endTransaction("Error during download!", true);
    }).always(function() {
        stopSpinner();
    });
}

var i;
var timer;
var prompt;
var spinner;
function startSpinner(term, spinner) {
    i = 0;
    function set() {
        var text = spinner.frames[i++ % spinner.frames.length];
        term.set_prompt(text);
    };
    prompt = term.get_prompt();
    set();
    timer = setInterval(set, spinner.interval);
}
function stopSpinner() {
    clearInterval(timer);
}