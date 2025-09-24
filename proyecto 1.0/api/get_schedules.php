<?php
    require_once('config.php')
    
    $pdo-> prepare('SELECT * FROM feeder_schedule ORDER BY updated_at DESC')
    $pdo-> execute()

?>
