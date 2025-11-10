<?php
ob_start();

try {
    require_once 'config.php';

    if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
        throw new Exception('Método no permitido');
    }

    // Tomar hora y mac
    $hora = $_POST['hora'] ?? '';
    $MAC = $_GET['mac'] ?? ($_POST['mac'] ?? '');

    if (empty($hora)) {
        throw new Exception('Hora es requerida');
    }

    if (empty($MAC)) {
        throw new Exception('MAC no proporcionada');
    }

    // Validar formato de hora (HH:MM o HH:MM:SS)
    if (!preg_match('/^([01]?[0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9])?$/', $hora)) {
        throw new Exception('Formato de hora inválido. Use HH:MM o HH:MM:SS');
    }

    // Buscar el alimentador por MAC
    $stmt = $pdo->prepare("SELECT * FROM alimentador WHERE mac = ?");
    $stmt->execute([$MAC]);
    $result = $stmt->fetch(PDO::FETCH_ASSOC);

    if ($result) {
        $pdo->beginTransaction();

        // Actualizar la hora programada
        $stmt = $pdo->prepare("UPDATE alimentador SET scheduled_time = :hora, updated_at = NOW() WHERE mac = :mac");
        $stmt->execute([':hora' => $hora, ':mac' => $MAC]);

        $pdo->commit();

        ob_clean();
        echo json_encode([
            'success' => true,
            'message' => 'Horario guardado exitosamente',
            'hora' => $hora
        ], JSON_UNESCAPED_UNICODE);
    } else {
        echo json_encode([
            'success' => false,
            'message' => 'El alimentador no existe en la base de datos',
            'hora' => null
        ], JSON_UNESCAPED_UNICODE);
    }
} catch (Exception $e) {
    if (isset($pdo) && $pdo->inTransaction()) {
        $pdo->rollback();
    }

    ob_clean();
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'message' => 'Error al guardar horario',
        'error' => $e->getMessage()
    ], JSON_UNESCAPED_UNICODE);
}

ob_end_flush();
?>