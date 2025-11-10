<?php
// Evitar cualquier output antes del JSON
ob_start();

try {
    require_once 'config.php';
    
    if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
        throw new Exception('Método no permitido');
    }

    $MAC = $_REQUEST['MAC'];

    $stmt = $pdo->prepare("SELECT scheduled_time FROM alimentador WHERE MAC = '$MAC' ORDER BY updated_at DESC LIMIT 1");
    $stmt->execute();
    $result = $stmt->fetch();

    // Limpiar cualquier output buffer
    ob_clean();
    
    if ($result && isset($result['scheduled_time'])) {
        echo json_encode([
            'success' => true,
            'hora' => $result['scheduled_time']
        ], JSON_UNESCAPED_UNICODE);
    } else {
        echo json_encode([
            'success' => false,
            'message' => 'No hay horario programado',
            'hora' => null
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

// Finalizar output buffer
ob_end_flush();
?>
