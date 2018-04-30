// for browsers that don't support key property
keyboardeventKeyPolyfill.polyfill();

var FIRMWARE_FILE = "/firmware.dc";
var FIRMWARE_EXTENSION = "dc";

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

function endTransaction(msg, iserror, cb) {
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
            if (typeof(cb) == "function") {
                cb();
            }
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
            getConfig(false, downloadFile);
        });
    /*} else if (command.match(/^\s*secureflash\s*$/)) {
        startTransaction(null, function() {
            flash(true);
        });*/
    } else if (command.match(/^\s*flash\s*$/)) {
        startTransaction(null, function() {
            flash();
        });
    } else if (command.match(/^\s*reset\s*$/)) {
        startTransaction(null, function() {
            reset();
        });
    } else if (command.match(/^\s*file\s*$/)) {
        startTransaction(null, function() {
            fileInputChange();
        });
    } else if (command.match(/^\s*get\s*$/)) {
        startTransaction(null, function() {
            getConfig(false, getFirmwareData);
        });
    } else if (command.match(/^\s*check\s*$/)) {
        startTransaction(null, function() {
            getConfig(false, getFirmwareData);
        });
    } else if (command.match(/^\s*restart\s*$/)) {
        startTransaction(null, function() {
            restartESP();
        });
    } else if (command.match(/^\s*setup\s*$/)) {
        startTransaction(null, function() {
            getConfig(false, setupMode);
        });
    } else if (command.match(/^\s*config\s*$/)) {
        startTransaction(null, function() {
            getConfig(true);
        });
    } else if (command.match(/^\s*details\s*$/)) {
        helpDetails();
    } else if (command.match(/^\s*banner\s*$/)) {
        term.clear();
        term.greetings();
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
        typed_message(term, 'Type [[b;#fff;]help] to get help!\n', 36, function() {
            startTransaction(null, function() {
                checkSetupStatus();
            });
        });
    },
    completion: [
        "help",
        "get",
        "check",
        "select",
        "file",
        "upload",
        "download",
        "flash",
        /*"secureflash",*/
        "reset",
        "details",
        "setup",
        "config",
        "clear",
        "banner",
        "restart",
        "exit"
    ],
    prompt: 'dc-hdmi> ',
    greetings: [
        '       __                                        __ ',
        '   ___/ /___ _____ ___   ______ ____ ___   ____ / /_',
        '  / _  // _// _  // _ \\ /     // __// _ \\ /  _// __/',
        ' / // // / / ___// // // / / // /_ / // /_\\ \\ / /__ ',
        ' \\___//_/ /____/ \\__\\_/_/ /_//___/ \\__\\_/___/ \\___/',
        '                            __  __ __   ______ __',
        '                           / __/ //  \\ /     // /',
        '     @citrus3000psi       / __  // / // / / // /',
        '       @chriz2600        /_/ /_//___//_/ /_//_/',
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

function helpDetails() {
    var msg = "";
    msg = "[[b;#fff;]Firmware upgrade procedure:]\n"
        + "  ________          ___________         _________\n"
        + " /        \\        /           \\ flash /         \\\n"
        + " | jQuery |        |   esp-07  |------>|  FPGA   |\n"
        + " |  Term  | upload |  staging  | reset |  flash  |\n"
        + " | (this) |------->|   flash   |------>|  (SPI)  |\n"
        + " \\________/        \\___________/       \\_________/\n"
        + "   |   /|\\              /|\\\n"
        + "   |    |       download |\n"
        + "  \\|/   |           _____|_____\n"
        + "   select          /           \\\n"
        + "   (from           | dc.i74.de |\n"
        + "     HD)           \\___________/\n"
        + " \n";
    typed_message(term, msg, 1);
}

function help(full) {
    var msg = "";
    if (full) {
        msg = " \n"
            + "To [[b;#fff;]flash] the firmware, you have 2 options:\n"
            + " \n"
            + "1) [[b;#fff;]select] a firmware from your harddisk and [[b;#fff;]upload] it\n"
            + "   to the staging area\n"
            + "2) [[b;#fff;]download] the latest firmware from dc.i74.de to the\n"
            + "   staging area\n"
            + " \n"
            + "After that, you are ready to [[b;#fff;]flash] the firmware to\n"
            + "the FPGA configuration memory. It's possible to re-flash\n"
            + "the firmware from the staging area at any time, because\n"
            + "it's stored in the flash of the WiFi chip.\n"
            + "Then type [[b;#fff;]reset] to reset the FPGA and load the previously\n"
            + "flashed firmware.\n"
            + " \n"
            + "Type [[b;#fff;]details] to show a diagram of the upgrade procedure.\n"
            + " \n";
    }
    msg += "Commands:\n";
    msg += "[[b;#fff;]get]:         get checksum of installed firmware\n";
    msg += "[[b;#fff;]check]:       check if new firmware is available\n";
    msg += "[[b;#fff;]select]:      select file to upload\n";
    msg += "[[b;#fff;]file]:        show information on selected file\n";
    msg += "[[b;#fff;]upload]:      upload selected file\n";
    msg += "[[b;#fff;]download]:    download latest flash file from dc.i74.de\n";
    msg += "[[b;#fff;]flash]:       flash FPGA from staging area\n";
    /*msg += "[[b;#fff;]secureflash]: flash FPGA from staging area, while disabling fpga\n";*/
    msg += "[[b;#fff;]reset]:       reset FPGA\n";
    msg += "[[b;#fff;]details]:     show firmware upgrade procedure\n";
    msg += "[[b;#fff;]setup]:       enter setup mode\n";
    msg += "[[b;#fff;]config]:      get current setup\n";
    msg += "[[b;#fff;]clear]:       clear terminal screen\n";
    msg += "[[b;#fff;]restart]:     restarts ESP module\n";
    msg += "[[b;#fff;]exit]:        end terminal\n";

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

function uploadFile(isRetry) {
    var file = $("#fileInput").get(0).files[0];
    if (!file) {
        endTransaction("No file selected!", true);
        return;
    }
    var formData = new FormData();
    var client = new XMLHttpRequest();

    formData.append("file", file);

    client.onerror = function(e) {
        if (isRetry) {
            endTransaction("Error during upload!", true);
        } else {
            uploadFile(true);
        }
    };

    client.onload = function(e) {
        if (client.status >= 200 && client.status < 400) {
            $.ajax(FIRMWARE_FILE + ".md5").done(function (data) {
                endTransactionWithMD5Check(lastMD5, data, "Please try to re-download file.");
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

var setupData = {};
var currentConfigData = {};
var setupDataMapping = {
    ssid:             [ "WiFi SSID        ", "empty" ],
    password:         [ "WiFi Password    ", "empty" ],
    ota_pass:         [ "OTA Password     ", "empty" ],
    firmware_server:  [ "Firmware Server  ", "dc.i74.de" ],
    firmware_version: [ "Firmware Version ", "master" ],
    firmware_fpga:    [ "Firmware FPGA    ", "10CL025" ],
    firmware_format:  [ "Firmware Format  ", "VGA" ],
    http_auth_user:   [ "HTTP User        ", "Test" ],
    http_auth_pass:   [ "HTTP Password    ", "testtest" ]
};

function setupDataDisplayToString(data, isSafe) {
    var value = " \n";
    for (x in data) {
        var t = setupDataMapping[x][0] || x;
        value += t + ": " 
            + (
                data[x] 
                ? ('[[b;#fff;]' + data[x] + ']')
                : (isSafe ? '[[b;yellow;]reset]' : '[[b;red;]not yet set]')
            )
             + " \n";
    }
    return value;
}

function prepareQuestion(pos, total, field) {
    return { 
        q : pos + '/' + total + ': ' + setupDataMapping[field][0] 
        + " \n    (default: [[b;yellow;]" + setupDataMapping[field][1] + "])"
        + " \n    (current value: " 
        + (currentConfigData[field] 
            ? "[[b;green;]" + currentConfigData[field] + "]"
            : "[[b;red;]not yet set]"
        )
        + ")"
        + " \n    New value? ", 
        cb: function(value) { 
            setupData[field] = value; 
        }
    };
}

function restartESP() {
    $.ajax("/restart");
    endTransaction('ESP module restarted.');
}

function getConfig(show, cb) {
    $.ajax("/config").done(function (data) {
        currentConfigData = data;
        endTransaction(
            show ? " \n-- Current config: ----------------------------------"
            + setupDataDisplayToString(currentConfigData, false)
            + "-----------------------------------------------------" : ""
        , null, cb);
    }).fail(function() {
        endTransaction('Error getting current config.', true);
    });
}

function setupMode() {
    term.history().disable();
    var questions = [
        {
            pc: function() {
                if (JSON.stringify(setupData) == JSON.stringify({})) {
                    term.error("no changes made.");
                    return false;
                }
                return true;
            },
            q : function() {
                return " \n-- Changes to save: ---------------------------------"
                    + setupDataDisplayToString(setupData, true)
                    + "-----------------------------------------------------"
                    + ' \nAfter saving changes, you will have to'
                    + ' \nrestart this application by typing: [[b;#fff;]restart].'
                    + ' \nSave changes (y)es/(n)o? ';
            },
            cb: function(value) {
                if (value.match(/^(y|yes)$/)) {
                    // start saving transaction
                    startTransaction("saving setup...", function() {
                        $.ajax({
                            type: "POST",
                            url: "/setup",
                            data: $.param(setupData, true)
                        }).done(function (data) {
                            endTransaction('[[b;#fff;]Done] Setup data saved.\n');
                        }).fail(function() {
                            endTransaction('Error saving setup data.', true);
                        });
                    });
                } else {
                    term.error("discarded setup");
                }
                // reset setupData
                setupData = {};
            }
        }
    ];
    var keyz = Object.keys(setupDataMapping);
    var size = keyz.length;
    for (var i = size - 1 ; i >= 0 ; i--) {
        questions.unshift(prepareQuestion(i + 1, size, keyz[i]));
    }
    var next = function() {
        var n = questions.shift();
        var v;
        if (n) {
            n.pc = n.pc || function() { return true; };
            if (n.pc()) {
                term.push(function(command) {
                    term.pop();
                    var lm = command.match(/^(.+)$/i);
                    if (lm) {
                        lm[0] = $.trim(lm[0]);
                        if (n.cb) {
                            n.cb(lm[0]);
                        }
                    }
                    next();
                }, {
                    prompt: typeof(n.q) == "function" ? n.q() : n.q
                });
            } else {
                term.history().enable();
            }
        } else {
            term.history().enable();
        }
    }

    term.echo(" \n[[b;#fff;]This will guide you through the setup process:]\n"
        + "- Just hit return to leave the value unchanged.\n"
        + "- Enter a single space to reset value to firmware default.\n"
        + "- CTRL-D to abort.\n"
    );
    next();
}

function checkSetupStatus() {
    $.ajax("/issetupmode").done(function (data) {
        var setupStatus = $.trim(data);
        if (setupStatus === "false") {
            endTransaction();
        } else {
            endTransaction(null, null, function() { 
                getConfig(false, setupMode); 
            });
        }
    }).fail(function() {
        endTransaction("Error checking setup status!", true);
    });
}

function _getMD5File() {
    return (
          "//" + currentConfigData["firmware_server"]
        + "/fw/" + currentConfigData["firmware_version"]
        + "/DCxPlus-" + currentConfigData["firmware_fpga"]
        + "-" + currentConfigData["firmware_format"]
        + "." + FIRMWARE_EXTENSION + ".md5?cc=" + Math.random()
    );
}

function getFirmwareData() {
    $.ajax("/etc/last_flash_md5").done(function (data) {
        var lastFlashMd5 = $.trim(data);
        $.ajax(FIRMWARE_FILE + ".md5").done(function (data) {
            var stagedMd5 = $.trim(data);
            $.ajax(_getMD5File()).done(function (data) {
                var origMd5 = $.trim(data);
                endTransaction(
                    'Installed firmware:\n'
                  + ' MD5: ' + (lastFlashMd5 == "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" ? "[[b;yellow;]No previously flashed firmware found]" : '[[b;#fff;]' + lastFlashMd5 + ']') + '\n'
                  + 'Staged firmware:\n'
                  + ' MD5: ' + (stagedMd5 == "00000000000000000000000000000000" ? "[[b;yellow;]No staged firmware found]" : '[[b;#fff;]' + stagedMd5 + ']') + '\n'
                  + 'Official firmware:\n'
                  + ' MD5: [[b;#fff;]' + origMd5 + ']\n'
                  + '\n'
                  + ((lastFlashMd5 == origMd5)
                    ? 'Currently installed firmware is already the latest version.'
                    : 'New firmware available.')
                );
            }).fail(function() {
                endTransaction('Error reading original checksum', true);
            });
        }).fail(function() {
            endTransaction("Error reading checksum!", true);
        });
    }).fail(function() {
        endTransaction("Error reading checksum!", true);
    });

}
function checkFirmware() {
    $.ajax("/etc/last_flash_md5").done(function (data) {
        var lastFlashMd5 = $.trim(data);
        $.ajax(_getMD5File()).done(function (data) {
            var origMd5 = $.trim(data);
            if (lastFlashMd5 == origMd5) {
                endTransaction('Currently installed firmware is already the latest version.');
            } else {
                endTransaction('New firmware available.');
            }
        }).fail(function() {
            endTransaction('Error reading original checksum', true);
        });
    }).fail(function() {
        endTransaction("Error resetting fpga!", true);
    });
}

function reset() {
    $.ajax("/reset").done(function (data) {
        endTransaction('FPGA reset [[b;green;]OK]\n');
    }).fail(function() {
        endTransaction("Error resetting fpga!", true);
    });
}

function doProgress(successCallback) {
    function progressPoll() {
        $.ajax("/progress").done(function (data) {
            var pgrs = $.trim(data);
            if (pgrs.indexOf("ERROR") == 0) {
                endTransaction(pgrs, true)
            } else {
                term.set_prompt(progress(pgrs, progressSize));
                if (pgrs == "100") {
                    successCallback();
                } else {
                    setTimeout(progressPoll, 1500);
                }
            }
        }).fail(function () {
            endTransaction('Error reading progress', true);
        })
    };
    setTimeout(progressPoll, 1500);
}

function downloadFile() {
    //startSpinner(term, spinners["shark"]);
    term.set_prompt(progress(0, progressSize));
    $.ajax("/download").done(function (data) {
        doProgress(
            function() {
                $.ajax(FIRMWARE_FILE + ".md5").done(function (data) {
                    var calcMd5 = $.trim(data);
                    $.ajax(_getMD5File()).done(function (data) {
                        var origMd5 = $.trim(data);
                        endTransactionWithMD5Check(calcMd5, origMd5, "Please try to re-download file.");
                    }).fail(function() {
                        endTransaction('Error reading original checksum', true);
                    });
                }).fail(function() {
                    endTransaction('Error reading checksum', true);
                });
            }
        );
    }).fail(function() {
        endTransaction("Error starting download!", true);
    });
}

function flash(secure) {
    //startSpinner(term, spinners["shark"]);
    term.set_prompt(progress(0, progressSize));
    $.ajax(secure ? "/secureflash" : "/flash").done(function (data) {
        doProgress(
            function() {
                $.ajax(FIRMWARE_FILE + ".md5").done(function (data) {
                    var calcMd5 = $.trim(data);
                    $.ajax("/etc/last_flash_md5").done(function (data) {
                        var lastFlashMd5 = $.trim(data);
                        endTransactionWithMD5Check(calcMd5, lastFlashMd5, "Please try to re-flash.");
                    }).fail(function() {
                        endTransaction('Error reading checksum', true);
                    });
                }).fail(function() {
                    endTransaction('Error reading checksum', true);
                });
            }
        );
    }).fail(function() {
        endTransaction("Error starting flash process!", true);
    });
}

/*function flash(secure) {
    term.set_prompt(progress(0, progressSize));

    var client = new XMLHttpRequest();

    client.onerror = function(e) {
        endTransaction('[[b;red;]FAIL]\n');
    };

    client.onload = function() {
        term.set_prompt(progress(100, progressSize));
        $.ajax(FIRMWARE_FILE + ".md5").done(function (data) {
            var calcMd5 = $.trim(data);
            $.ajax("/etc/last_flash_md5").done(function (data) {
                var lastFlashMd5 = $.trim(data);
                endTransactionWithMD5Check(calcMd5, lastFlashMd5, "Please try to re-flash.");
            }).fail(function() {
                endTransaction('Error reading checksum', true);
            });
        }).fail(function() {
            endTransaction('Error reading checksum', true);
        });
    };

    client.onreadystatechange = function() {
        if (client.readyState == 3) {
            var lines = client.responseText.split(/\n/);
            lines.pop();
            var pgrs = $.trim(lines.pop());
            term.set_prompt(progress(pgrs, progressSize));
        }
    };

    client.onabort = function(e) {
        endTransaction("Flashing aborted!", true);
    };


    client.open("GET", secure ? "/secureflash" : "/flash");
    client.send(null);

}
*/
function endTransactionWithMD5Check(chk1, chk2, msg) {
    endTransaction(
        progress(100, progressSize) + ' [[b;green;]OK]\n'
        + "MD5 Check:\n"
        + (chk1 == chk2
            ? "    " + chk1 + "\n == " + chk2 + " [[b;green;]OK]"
            : "    " + chk1 + "\n[[b;red;] != ]" + chk2 + " [[b;red;]FAIL]")
        + (chk1 == chk2 ? "" : "\n" + msg)
    );
}

/*
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
*/