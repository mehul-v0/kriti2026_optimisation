const express = require('express');
const multer = require('multer');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const cors = require('cors');

const app = express();

// 1. NUCLEAR CORS FIX: Allow EVERYTHING
app.use(cors({
    origin: '*',
    methods: ['GET', 'POST', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization']
}));

// 2. Storage Config (Unique Filenames)
const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        const dir = 'uploads/';
        if (!fs.existsSync(dir)) fs.mkdirSync(dir);
        cb(null, dir);
    },
    filename: (req, file, cb) => {
        // Simple safe filename
        cb(null, `file-${Date.now()}-${file.originalname}`);
    }
});
const upload = multer({ storage: storage });

// 3. The Processing Route
app.post('/process-vrp', upload.single('excelFile'), (req, res) => {
    // Debug Log: Prove the request hit the server
    console.log(`[Request Received] From: ${req.ip}`);

    if (!req.file) {
        console.error("Error: No file uploaded.");
        return res.status(400).json({ error: 'No file uploaded' });
    }

    const inputPath = req.file.path;
    // Fix: Use absolute path for output to avoid Python path issues
    const outputPath = path.resolve(__dirname, 'uploads', `output_${Date.now()}.json`);

    console.log(`[Processing] ${req.file.originalname}`);
    console.log(`[Paths] In: ${inputPath} | Out: ${outputPath}`);

    // 4. Spawn Python (With Error Handling for missing Python)
    const python = spawn('python', ['vrp_solver.py', inputPath, outputPath]);

    python.stdout.on('data', (data) => console.log(`[Python Log]: ${data}`));
    python.stderr.on('data', (data) => console.error(`[Python Err]: ${data}`));

    python.on('error', (err) => {
        console.error("[CRITICAL] Failed to start Python:", err);
        return res.status(500).json({ error: "Server cannot run Python. Is Python installed?" });
    });

    python.on('close', (code) => {
        if (code !== 0) {
            console.error(`[Fail] Python exited with code ${code}`);
            // cleanup([inputPath, outputPath]); // Keep files for debugging if it fails
            return res.status(500).json({ error: 'Optimization Algorithm Failed. Check Server Logs.' });
        }

        fs.readFile(outputPath, 'utf8', (err, data) => {
            if (err) {
                console.error("[Fail] Output file not found.");
                return res.status(500).json({ error: 'Output read failed' });
            }
            try {
                const jsonResponse = JSON.parse(data);
                console.log("[Success] Sending JSON response.");
                res.json(jsonResponse);
            } catch (e) {
                console.error("[Fail] Invalid JSON.");
                res.status(500).json({ error: 'Invalid JSON output' });
            } finally {
                // cleanup([inputPath, outputPath]); // Uncomment to save space later
            }
        });
    });
});

function cleanup(files) {
    files.forEach(f => { if (fs.existsSync(f)) fs.unlinkSync(f); });
}

const PORT = 3000;
app.listen(PORT, () => console.log(`Server running on http://localhost:${PORT}`));