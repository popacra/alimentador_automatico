<?php
ob_start();

try {
    require_once 'config.php';

    if ($_SERVER['REQUEST_METHOD'] !== "POST") {
        throw new Exception('Método no permitido');
    } else {

        $data = json_decode(file_get_contents("php://input"), true);
        if (!$data || !isset($data["momento"], $data["fecha"], $data["hora"], $data["descripcion"])) {
            echo json_encode(["error" => "JSON inválido o campos incompletos"]);
            exit;
        }

        $stmt = $pdo->prepare('INSERT INTO historial (momento, fecha, hora, descripcion) VALUES(?,?,?,?) ORDER BY id DESC');
        $stmt->execute([$data['momento'], $data['fecha'], $data['hora'], $data['descripcion']]);


        echo json_encode([
            "success" => true,
            "message" => "Datos insertados correctamente",
            "data" => $data
        ]);
    }
} catch (PDOException $e) {
    echo json_encode(["error" => "Error en la base de datos: " . $e->getMessage()]);
}


?>