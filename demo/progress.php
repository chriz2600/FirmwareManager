<?php
    if (session_status() == PHP_SESSION_NONE) {
        session_start();
    }

    $_SESSION["progress"] = $_SESSION["progress"] + 10;
    echo $_SESSION["progress"];