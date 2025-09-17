<?php
// Evitar cualquier output antes del JSON
ob_start();

try {
    require_once 'config.php';
    
    if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
        throw new Exception('Método no permitido');
    }

    $hora = $_POST['hora'] ?? '';
    
    if (empty($hora)) {
        throw new Exception('Hora es requerida');
    }

    // Validar formato de hora (HH:MM:SS)
    if (!preg_match('/^([01]?[0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]$/', $hora)) {
        throw new Exception('Formato de hora inválido. Use HH:MM:SS');
    }

    // Iniciar transacción
    $pdo->beginTransaction();

    // Eliminar horarios anteriores
    $stmt = $pdo->prepare("DELETE FROM feeder_schedule");
    $stmt->execute();

    // Insertar nuevo horario
    $stmt = $pdo->prepare("INSERT INTO feeder_schedule (scheduled_time, updated_at) VALUES (?, NOW())");
    $stmt->execute([$hora]);

    // Confirmar transacción
    $pdo->commit();

    // Limpiar output buffer
    ob_clean();
    
    echo json_encode(value: [
        'success' => true,
        'message' => 'Horario guardado exitosamente',
        'hora' => $hora
    ], flags: JSON_UNESCAPED_UNICODE);

} catch (Exception $e) {
    // Rollback en caso de error
    if (isset($pdo) && $pdo->inTransaction()) {
        $pdo->rollback();
    }
    
    // Limpiar output buffer
    ob_clean();
    
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'message' => 'Error al guardar horario',
        'error' => $e->getMessage()
    ], flags: JSON_UNESCAPED_UNICODE);
}

// Finalizar output buffer
ob_end_flush();
?>
