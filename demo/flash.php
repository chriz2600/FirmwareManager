<?php
    if (session_status() == PHP_SESSION_NONE) {
        session_start();
    }

    ob_implicit_flush(1);
    header('X-Accel-Buffering: no');

    for ($i = 0 ; $i <= 100 ; $i += 2) {
        echo "$i\n";
        usleep(70000);
    }

    $_SESSION["last_flash_md5"] = $_SESSION["firmware.dc.md5"] ? $_SESSION["firmware.dc.md5"] : "751bfdeb1793261853887dc11c876d40";
    