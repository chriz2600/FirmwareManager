<?php
    if (session_status() == PHP_SESSION_NONE) {
        session_start();
    }

    $_SESSION["progress"] = 0;
    $_SESSION["firmware.dc.md5"] = file_get_contents("http://dc.i74.de/firmware.dc.md5");
