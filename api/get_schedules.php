<?php
require_once('config.php');
ob_start();

try {

    $stmt = $pdo->prepare('SELECT * FROM feeder_schedule ORDER BY updated_at DESC');
    $stmt->execute();
    $results = $stmt->fetchAll();

    // Limpiar cualquier output buffer
    ob_clean();

    if ($results && count($results)>0) {
        $horarios = array_map(function($row){
            return $row['scheduled_time'];
        },$results);
        echo json_encode([
            'success' => true,
            'horarios' => $horarios
        ], JSON_UNESCAPED_UNICODE);
    } else {
        echo json_encode([
            'success' => false,
            'message' => 'No hay horario programado',
            'horarios' => null
        ], JSON_UNESCAPED_UNICODE);
    }



} catch (Exception $e) {
    // Limpiar output buffer en caso de error
    ob_clean();

    http_response_code(500);
    echo json_encode([
        'success' => false,
        'message' => 'Error del servidor',
        'error' => $e->getMessage()
    ], JSON_UNESCAPED_UNICODE);
}

?>