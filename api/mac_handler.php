<?php
ob_start();

try {
    require_once 'config.php';

    if ($_SERVER['REQUEST_METHOD'] === "POST") {
        $data = json_decode(file_get_contents("php://input"), true);
        if (!$data || !isset($data["mac"])) {
            echo json_encode(["success" => false, "message" => "JSON inválido o campos incompletos"]);
            exit;
        }
        $mac = $data['mac'];
        $stmt = $pdo->prepare('SELECT * FROM alimentador WHERE MAC = ?');
        $stmt->execute([$mac]);
        $result = $stmt->fetch();

        if ($result) {
            echo json_encode([
                "success" => true,
                "exists" => true,
                "mac" => $mac,
                "message" => "MAC ya registrada previamente"
            ]);
        } else {
            $stmt = $pdo->prepare('INSERT INTO alimentador (MAC) VALUES (?)');
            $stmt->execute([$mac]);

            echo json_encode([
                "success" => true,
                "exists" => true,
                "mac" => $mac,
                "message" => "Nueva MAC registrada correctamente"
            ]);
        }
        exit;
    }
    if ($_SERVER['REQUEST_METHOD'] === "GET") {
        $mac = $_GET['mac'] ?? '';

        if (empty($mac)) {
            echo json_encode(["success" => false, "message" => "MAC no proporcionada"]);
            exit;
        }

        $stmt = $pdo->prepare('SELECT * FROM alimentador WHERE MAC = ?');
        $stmt->execute([$mac]);
        $result = $stmt->fetch(PDO::FETCH_ASSOC);

        if ($result) {
            echo json_encode([
                "success" => true,
                "exists" => true,
                "mac" => $mac
            ]);
        } else {
            echo json_encode([
                "success" => true,
                "exists" => false,
                "message" => "MAC no registrada"
            ]);
        }
        exit;
    }


    echo json_encode(["success" => false, "message" => "Método no permitido"]);


} catch (PDOException $e) {
    echo json_encode(["error" => "Error en la base de datos: " . $e->getMessage()]);
}


?>