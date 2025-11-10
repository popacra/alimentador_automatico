<?php
// Evitar cualquier output antes del JSON
ob_start();
ini_set('display_errors', 1);
error_reporting(E_ALL);

try {
    require_once 'config.php';

    if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
        throw new Exception('Método no permitido');
    }

    // Recibir la MAC
    $mac = $_GET['mac'] ?? '';

    if (empty($mac)) {
        throw new Exception('MAC no especificada');
    }

    // Buscar horario en la tabla de alimentadores
    $stmt = $pdo->prepare("SELECT scheduled_time FROM alimentador WHERE mac = ?");
    $stmt->execute([$mac]);
    $result = $stmt->fetch(PDO::FETCH_ASSOC);

    ob_clean();

    if ($result && !empty($result['scheduled_time'])) {
        echo json_encode([
            'success' => true,
            'hora' => $result['scheduled_time'],
            'mac' => $mac
        ], JSON_UNESCAPED_UNICODE);
    } else {
        echo json_encode([
            'success' => false,
            'message' => 'No hay horario programado',
            'hora' => null
        ], JSON_UNESCAPED_UNICODE);
    }

} catch (Exception $e) {
    ob_clean();
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'message' => 'Error del servidor',
        'error' => $e->getMessage()
    ], JSON_UNESCAPED_UNICODE);
}

ob_end_flush();
?>