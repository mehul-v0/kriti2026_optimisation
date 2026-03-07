import { useState, useCallback, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { uploadFile } from '../services/api';
import { mapEmployees, mapVehicles } from '../utils/mappers';
import LoadingSpinner from '../components/LoadingSpinner';
import type { Employee, Vehicle } from '../types';

export default function DataUpload() {
  const navigate = useNavigate();
  const [uploadedFile, setUploadedFile] = useState<File | null>(null);
  const [parsedData, setParsedData] = useState<{ employees: Employee[]; vehicles: Vehicle[] } | null>(null);
  const [isValidating, setIsValidating] = useState(false);
  const [validationError, setValidationError] = useState<string | null>(null);
  const [dragActive, setDragActive] = useState(false);

  useEffect(() => {
    const savedData = sessionStorage.getItem('uploadedData');
    if (savedData) {
      try {
        const parsed = JSON.parse(savedData);
        if (parsed.employees && parsed.vehicles) {
          setParsedData({ employees: parsed.employees, vehicles: parsed.vehicles });
          setUploadedFile(new File([''], parsed.filename || 'uploaded-data.xlsx', { type: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet' }));
        }
      } catch (e) {
        console.warn('Failed to load saved data:', e);
      }
    }
  }, []);

  const handleDrag = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  }, []);

  const processFile = async (file: File) => {
    setIsValidating(true);
    setValidationError(null);

    try {
      const response = await uploadFile(file);

      if (!response.success) {
        throw new Error(response.message || 'Backend failed to process the file');
      }

      const employees = mapEmployees(response.employees);
      const vehicles = mapVehicles(response.vehicles);

      if (employees.length === 0) {
        throw new Error('No employees found in the Excel file. Please check the Employees sheet contains valid data.');
      }
      
      if (vehicles.length === 0) {
        throw new Error('No vehicles found in the Excel file. Please check the Vehicles sheet contains valid data.');
      }

      setParsedData({ employees, vehicles });

      sessionStorage.setItem('uploadedData', JSON.stringify({
        employees,
        vehicles,
        backendEmployees: response.employees,
        backendVehicles: response.vehicles,
        baselineCost: response.baseline_cost,
        filename: response.filename,
        digest: response.digest,
      }));

      sessionStorage.removeItem('optimizationComplete');
      sessionStorage.removeItem('shouldRunOptimization');

      setIsValidating(false);
    } catch (error: any) {
      let errorMsg = error.message || 'Failed to upload and parse file';
      if (errorMsg.includes('Excel conversion failed')) {
        errorMsg = errorMsg.replace('Excel conversion failed: ', '');
      }
      setValidationError(errorMsg);
      setParsedData(null);
      setIsValidating(false);
    }
  };

  const handleDrop = useCallback(async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setDragActive(false);

    const files = e.dataTransfer.files;
    if (files && files[0]) {
      setParsedData(null);
      setValidationError(null);
      setUploadedFile(files[0]);
      await processFile(files[0]);
    }
  }, []);

  const handleFileSelect = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (files && files[0]) {
      setParsedData(null);
      setValidationError(null);
      setUploadedFile(files[0]);
      await processFile(files[0]);
    }
  };

  const handleRemove = () => {
    setUploadedFile(null);
    setParsedData(null);
    setValidationError(null);
    sessionStorage.removeItem('uploadedData');
    sessionStorage.removeItem('optimizationComplete');
    sessionStorage.removeItem('shouldRunOptimization');
  };

  const handleContinue = () => {
    if (parsedData) {
      navigate('/insights');
    }
  };

  return (
    <div className="h-[calc(100vh-48px)] overflow-hidden p-4 md:p-6">
      <div className="max-w-[1400px] mx-auto h-full flex flex-col md:flex-row gap-5">

        {/* ── Left Panel: Upload Area ── */}
        <div className="flex-1 flex flex-col bg-panel-dark border border-white/10 p-6">
          <div className="mb-4">
            <h1 className="text-2xl font-black text-white tracking-tight uppercase">Upload Your Data</h1>
            <p className="text-xs font-mono text-white/30 mt-1">Import employee request and vehicle fleet data to begin optimization</p>
          </div>

          {/* Drag & Drop Zone */}
          <motion.div
            className={`relative flex-1 flex flex-col items-center justify-center border border-dashed transition-all ${
              dragActive
                ? 'border-primary/60 bg-primary/5'
                : 'border-white/10 hover:border-white/20'
            }`}
            style={dragActive ? {} : { animation: 'pulse-border 3s ease-in-out infinite' }}
            onDragEnter={handleDrag}
            onDragLeave={handleDrag}
            onDragOver={handleDrag}
            onDrop={handleDrop}
          >
            <span className={`material-symbols-outlined text-4xl mb-3 transition-colors ${
              dragActive ? 'text-primary' : 'text-white/20'
            }`}>upload_file</span>

            <h3 className="text-sm font-label font-bold uppercase tracking-widest text-white/60 mb-1">
              Drag and drop your Excel file here
            </h3>
            <p className="text-[10px] font-mono text-white/30 mb-5">Supports .xlsx and .xls formats</p>

            {/* Upload buttons */}
            <div className="flex items-center gap-3">
              <label className="bg-primary text-background-dark font-label font-bold px-6 py-2 text-[11px] tracking-widest uppercase cursor-pointer transition-all glow-amber inline-flex items-center gap-2">
                <input type="file" accept=".xlsx,.xls" onChange={handleFileSelect} className="hidden" />
                <span className="material-symbols-outlined text-[16px]">description</span>
                Select Excel File
              </label>

              <div className="relative group">
                <button
                  className="w-9 h-9 border border-white/10 bg-white/[0.02] hover:bg-white/5 flex items-center justify-center transition-all"
                  onClick={() => {}}
                >
                  <span className="material-symbols-outlined text-white/30 text-[18px]">cloud_upload</span>
                </button>
                <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 bg-panel-dark border border-white/10 text-white text-[9px] font-mono px-3 py-1 opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                  Google Drive
                </div>
              </div>

              <div className="relative group">
                <button
                  className="w-9 h-9 border border-white/10 bg-white/[0.02] hover:bg-white/5 flex items-center justify-center transition-all"
                  onClick={() => {}}
                >
                  <span className="material-symbols-outlined text-white/30 text-[18px]">hard_drive</span>
                </button>
                <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 bg-panel-dark border border-white/10 text-white text-[9px] font-mono px-3 py-1 opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                  Dropbox
                </div>
              </div>
            </div>
          </motion.div>
        </div>

        {/* ── Right Panel: Status & Info ── */}
        <div className="w-full md:w-[380px] flex flex-col gap-4">

          {/* File Info Card */}
          <div className="bg-panel-dark border border-white/10 p-5 flex flex-col gap-4">
            <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">File Status</h2>

            {!uploadedFile && !isValidating && (
              <div className="flex flex-col items-center text-center py-6 text-white/20">
                <span className="material-symbols-outlined text-4xl mb-2">description</span>
                <p className="text-xs font-mono text-white/30">No file selected yet</p>
                <p className="text-[10px] font-mono mt-1 text-white/20">Upload an Excel file to get started</p>
              </div>
            )}

            {uploadedFile && (
              <motion.div initial={{ opacity: 0, y: 8 }} animate={{ opacity: 1, y: 0 }} className="flex items-center gap-3 bg-white/[0.02] p-3 border border-white/5">
                <span className="material-symbols-outlined text-primary text-2xl flex-shrink-0">description</span>
                <div className="flex-1 min-w-0">
                  <p className="font-mono text-white text-sm truncate">{uploadedFile.name}</p>
                  <p className="text-[10px] font-mono text-white/30">{(uploadedFile.size / 1024).toFixed(1)} KB</p>
                </div>
                <button onClick={handleRemove} className="text-action hover:text-action/80 transition-colors p-1 flex-shrink-0">
                  <span className="material-symbols-outlined text-[18px]">close</span>
                </button>
              </motion.div>
            )}

            {isValidating && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="py-2">
                <LoadingSpinner size="lg" text="Parsing and validating your data" className="py-2" />
              </motion.div>
            )}

            {validationError && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="bg-action/10 border-l-2 border-action p-3">
                <div className="flex items-start gap-2">
                  <span className="material-symbols-outlined text-action text-[16px] flex-shrink-0 mt-0.5">error</span>
                  <div>
                    <p className="font-label font-bold text-action text-xs uppercase tracking-wider">Validation Failed</p>
                    <p className="text-[10px] font-mono text-white/40 mt-0.5">{validationError}</p>
                  </div>
                </div>
              </motion.div>
            )}

            {parsedData && !validationError && (
              <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="bg-primary/5 border-l-2 border-primary p-3">
                <div className="flex items-start gap-2">
                  <span className="material-symbols-outlined text-primary text-[16px] flex-shrink-0 mt-0.5">check_circle</span>
                  <div>
                    <p className="font-label font-bold text-primary text-xs uppercase tracking-wider">Parsed Successfully</p>
                    <p className="text-[10px] font-mono text-white/40 mt-0.5">
                      {parsedData.employees.length} employees &middot; {parsedData.vehicles.length} vehicles
                    </p>
                  </div>
                </div>
              </motion.div>
            )}
          </div>

          {/* Data Summary Card — shown when parsed */}
          {parsedData && !validationError && (
            <motion.div initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} className="bg-panel-dark border border-white/10 p-5 flex flex-col gap-3">
              <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Data Summary</h2>
              <div className="grid grid-cols-2 gap-3">
                <div className="bg-white/[0.02] border border-white/5 p-3 text-center">
                  <span className="material-symbols-outlined text-primary text-2xl">groups</span>
                  <p className="text-xl font-bold font-mono text-white mt-1">{parsedData.employees.length}</p>
                  <p className="text-[10px] font-mono text-white/30">Employees</p>
                </div>
                <div className="bg-white/[0.02] border border-white/5 p-3 text-center">
                  <span className="material-symbols-outlined text-primary text-2xl">local_shipping</span>
                  <p className="text-xl font-bold font-mono text-white mt-1">{parsedData.vehicles.length}</p>
                  <p className="text-[10px] font-mono text-white/30">Vehicles</p>
                </div>
              </div>
            </motion.div>
          )}

          {/* Continue Button */}
          <div className="mt-auto">
            {parsedData && !validationError ? (
              <motion.button
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                onClick={handleContinue}
                className="w-full bg-primary text-background-dark font-label font-bold py-3 text-sm tracking-widest uppercase transition-all glow-amber inline-flex items-center justify-center gap-2"
              >
                Review Data Insights
                <span className="material-symbols-outlined text-lg">arrow_forward</span>
              </motion.button>
            ) : (
              <div className="bg-panel-dark border border-white/10 p-4 text-center">
                <p className="text-[10px] font-mono text-white/30">Upload & validate a file to continue</p>
              </div>
            )}
          </div>
        </div>

      </div>
    </div>
  );
}

