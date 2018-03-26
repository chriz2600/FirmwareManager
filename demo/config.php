<?php
    if (session_status() == PHP_SESSION_NONE) {
        session_start();
    }

    header('Content-Type: text/json');

    if (!$_SESSION["config"]) {
        $_SESSION["config"] = '{"ssid":"SomeSSID","password":"SomePassword","ota_pass":"SomeOTAPassword","firmware_server":"dc.i74.de","http_auth_user":"Test","http_auth_pass":"testtest"}';
    } else {
        error_log("c---> " . $_SESSION["config"]);
    }

    echo $_SESSION["config"];