import { useState, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Upload, Check, X, Cloud, HardDrive, FileSpreadsheet, ChevronDown } from 'lucide-react';
import { uploadFile } from '../services/api';
import { mapEmployees, mapVehicles } from '../utils/mappers';
import type { Employee, Vehicle } from '../types';

export default function DataUpload() {
  const navigate = useNavigate();
  const [uploadedFile, setUploadedFile] = useState<File | null>(null);
  const [parsedData, setParsedData] = useState<{ employees: Employee[]; vehicles: Vehicle[] } | null>(null);
  const [isValidating, setIsValidating] = useState(false);
  const [validationError, setValidationError] = useState<string | null>(null);
  const [dragActive, setDragActive] = useState(false);

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
      // Upload to backend — backend converts Excel → JSON and returns parsed data
      const response = await uploadFile(file);

      if (!response.success) {
        throw new Error('Backend failed to process the file');
      }

      // Map backend format to frontend types
      const employees = mapEmployees(response.employees);
      const vehicles = mapVehicles(response.vehicles);

      if (employees.length === 0 || vehicles.length === 0) {
        throw new Error('Excel file must contain both Employees and Vehicles data');
      }

      setParsedData({ employees, vehicles });

      // Store backend raw data for the optimization step
      sessionStorage.setItem('uploadedData', JSON.stringify({
        employees,
        vehicles,
        // Keep raw backend data for the optimize call
        backendEmployees: response.employees,
        backendVehicles: response.vehicles,
        baselineCost: response.baseline_cost,
        filename: response.filename,
        digest: response.digest,
      }));

      // Clear previous optimization state when new data is uploaded
      sessionStorage.removeItem('optimizationComplete');
      sessionStorage.removeItem('shouldRunOptimization');

      setIsValidating(false);
    } catch (error: any) {
      setValidationError(error.message || 'Failed to upload and parse file');
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
      setUploadedFile(files[0]);
      await processFile(files[0]);
    }
  }, []);

  const handleFileSelect = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (files && files[0]) {
      setUploadedFile(files[0]);
      await processFile(files[0]);
    }
  };

  const handleRemove = () => {
    setUploadedFile(null);
    setParsedData(null);
    setValidationError(null);
  };

  const handleContinue = () => {
    if (parsedData) {
      navigate('/insights');
    }
  };

  return (
    <div className="min-h-screen p-3">
      {/* Main Upload Section */}
      <section className="mb-3">
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-8">
          <div className="text-center mb-8">
            <h1 className="text-3xl font-bold text-white mb-2">Upload Your Data</h1>
            <p className="text-gray/70">Import employee request and vehicle fleet data to begin optimization</p>
          </div>

          {/* File Upload Area */}
          <motion.div
            className={`relative border-2 border-dashed rounded-2xl p-8 transition-all ${
              dragActive 
                ? 'border-primary/60 bg-primary/5' 
                : 'border-gray/30 hover:border-gray/50'
            }`}
            onDragEnter={handleDrag}
            onDragLeave={handleDrag}
            onDragOver={handleDrag}
            onDrop={handleDrop}
          >
            <div className="text-center">
              <Upload className={`w-12 h-12 mx-auto mb-4 mt-8 transition-colors ${
                dragActive ? 'text-primary' : 'text-gray/60'
              }`} />
              
              <h3 className="text-xl font-semibold text-white mb-2">
                Drag and drop your Excel file here
              </h3>
              <p className="text-sm text-gray/60 mb-6">Supports .xlsx and .xls formats</p>

              {/* Upload Options */}
              <div className="flex items-center justify-center gap-3 mb-8">
                {/* Main Select Button */}
                <label className="bg-primary hover:bg-primary-dark text-dark font-bold px-8 text-base rounded-xl cursor-pointer transition-all inline-flex items-center gap-2 w-80 justify-center h-14">
                  <input
                    type="file"
                    accept=".xlsx,.xls"
                    onChange={handleFileSelect}
                    className="hidden"
                  />
                  <FileSpreadsheet className="w-5 h-5" />
                  Select Excel File
                </label>

                {/* Google Drive Icon */}
                <div className="relative group">
                  <button 
                    className="w-14 h-14 bg-primary/10 hover:bg-primary/20 rounded-xl flex items-center justify-center transition-all"
                    onClick={() => {/* TODO: Implement Google Drive */}}
                  >
                    <Cloud className="w-7 h-7 text-primary" />
                  </button>
                  <div className="absolute top-full mt-2 left-1/2 transform -translate-x-1/2 bg-dark-700 text-white text-xs px-3 py-1 rounded opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                    Select Excel file from Google Drive
                  </div>
                </div>

                {/* Dropbox Icon */}
                <div className="relative group">
                  <button 
                    className="w-14 h-14 bg-primary/10 hover:bg-primary/20 rounded-xl flex items-center justify-center transition-all"
                    onClick={() => {/* TODO: Implement Dropbox */}}
                  >
                    <HardDrive className="w-7 h-7 text-primary" />
                  </button>
                  <div className="absolute top-full mt-2 left-1/2 transform -translate-x-1/2 bg-dark-700 text-white text-xs px-3 py-1 rounded opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap z-10">
                    Select Excel file from Dropbox
                  </div>
                </div>
              </div>
            </div>
          </motion.div>
        </div>
      </section>

      {/* Status Sections */}
      {(uploadedFile || isValidating || validationError || parsedData) && (
        <section className="mb-3">
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6 space-y-4">
            
            {/* File Confirmation Card */}
            {uploadedFile && (
              <motion.div
                initial={{ opacity: 0, y: 15 }}
                animate={{ opacity: 1, y: 0 }}
                className="bg-dark-700/50 rounded-xl p-4 border border-gray/5"
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-3">
                    <FileSpreadsheet className="w-10 h-10 text-primary" />
                    <div>
                      <p className="font-medium text-white">{uploadedFile.name}</p>
                      <p className="text-sm text-gray/60">
                        {(uploadedFile.size / 1024).toFixed(2)} KB • Excel File
                      </p>
                    </div>
                  </div>
                  <button
                    onClick={handleRemove}
                    className="text-red-400 hover:text-red-300 transition-colors p-1"
                  >
                    <X className="w-5 h-5" />
                  </button>
                </div>
              </motion.div>
            )}

            {/* Validation Loading */}
            {isValidating && (
              <motion.div
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                className="bg-dark-700/50 rounded-xl p-6 text-center border border-gray/5"
              >
                <div className="animate-spin w-8 h-8 border-3 border-primary border-t-transparent rounded-full mx-auto mb-3" />
                <p className="text-gray/80 text-sm">Parsing and validating file...</p>
              </motion.div>
            )}

            {/* Validation Error */}
            {validationError && (
              <motion.div
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                className="bg-red-500/10 border-red-500/30 rounded-xl p-4 border"
              >
                <div className="flex items-start gap-3">
                  <X className="w-5 h-5 text-red-400 flex-shrink-0 mt-0.5" />
                  <div>
                    <p className="font-medium text-red-400 mb-1">Validation Failed</p>
                    <p className="text-sm text-gray/80">{validationError}</p>
                  </div>
                </div>
              </motion.div>
            )}

            {/* Success Message */}
            {parsedData && !validationError && (
              <motion.div
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                className="bg-green-500/10 border-green-500/30 rounded-xl p-4 border"
              >
                <div className="flex items-start gap-3">
                  <Check className="w-5 h-5 text-green-400 flex-shrink-0 mt-0.5" />
                  <div>
                    <p className="font-medium text-green-400 mb-1">File parsed successfully!</p>
                    <p className="text-sm text-gray/80">
                      Found {parsedData.employees.length} employee requests and {parsedData.vehicles.length} vehicles
                    </p>
                  </div>
                </div>
              </motion.div>
            )}
          </div>
        </section>
      )}

      {/* Continue Action */}
      {parsedData && !validationError && (
        <section>
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6">
            <div className="flex justify-between items-center">
              <div>
                <h3 className="font-semibold text-white">Ready for Next Step</h3>
                <p className="text-sm text-gray/60">Your data has been successfully uploaded and validated</p>
              </div>
              <button
                onClick={handleContinue}
                className="bg-primary hover:bg-primary-dark text-dark font-bold px-6 py-3 rounded-lg transition-all hover:scale-105 inline-flex items-center gap-2"
              >
                Review Data Insights 
                <ChevronDown className="w-4 h-4 -rotate-90" />
              </button>
            </div>
          </div>
        </section>
      )}
    </div>
  );
}
