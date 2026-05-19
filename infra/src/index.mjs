import { S3Client, PutObjectCommand } from "@aws-sdk/client-s3";
const s3 = new S3Client({});

export const handler = async (event) => {
    try {
        if (!event.body) throw new Error("Missing payload body.");

        let body = typeof event.body === 'string' ? JSON.parse(event.body) : event.body;
        let encodedImage = body.image;
        
        if (!encodedImage) throw new Error("Missing 'image' key in JSON.");

        // 1. Extract the device ID passed explicitly by the ESP32 firmware
        // Defaults safely to 'unknown_node' if the key layout ever shifts
        const deviceId = body.deviceId || "unknown_node";

        // 2. Derive dynamic time parameters for the path hierarchy 
        const now = new Date();
        
        // Formats month cleanly as two digits (e.g., May -> '05')
        const month = String(now.getMonth() + 1).padStart(2, '0'); 
        
        // Formats day cleanly as two digits (e.g., 19th -> '19')
        const date = String(now.getDate()).padStart(2, '0');       
        
        // Millisecond precision Unix epoch timestamp suffix to completely rule out object collisions
        const timestamp = now.getTime(); 

        // 3. Assemble the target hierarchical path key matching your specification
        // Output Structure: node_01/05/19/farm_1779213456789.jpg
        const fileName = `${deviceId}/${month}/${date}/farm_${timestamp}.jpg`;

        // 4. Transform payload array back to binary byte layout 
        const imageBuffer = Buffer.from(encodedImage, 'base64');

        // 5. Transfer object payload directly into S3
        await s3.send(new PutObjectCommand({
            Bucket: "farm-supervisor-v1",
            Key: fileName,
            Body: imageBuffer,
            ContentType: "image/jpeg"
        }));

        return {
            statusCode: 200,
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ 
                message: "Binary Success", 
                file: fileName 
            })
        };
    } catch (error) {
        console.error("Storage Exception:", error);
        return {
            statusCode: 500,
            body: JSON.stringify({ error: error.message })
        };
    }
};
