<?php
    require_once('config.php')
    $stmt = $pdo-> prepare('SELECT * FROM feeder_schedule ORDER BY updated_at DESC');
    $stmt->execute();
    $result = $stmt->fetch(); 

    
?>

