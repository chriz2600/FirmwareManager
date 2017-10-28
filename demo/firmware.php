<?php
    if (session_status() == PHP_SESSION_NONE) {
        session_start();
    }

    if ($_SESSION["firmware.rbf.md5"]) {
        echo $_SESSION["firmware.rbf.md5"];
    } else {
        echo "751bfdeb1793261853887dc11c876d40";
    }
