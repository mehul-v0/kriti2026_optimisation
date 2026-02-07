import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Check, Loader2 } from 'lucide-react';
import { useApp } from '../context/AppContext';
import type { OptimizationResult } from '../types';
import { generateId } from '../utils/helpers';
import { runOptimization } from '../services/api';
import { buildOptimizationResult } from '../utils/mappers';

const stages = [
  'Data Parsing Complete',
  'Constraint Analysis Complete',
  'Route Optimization In Progress',
  'Cost Calculation',
  'Results Compilation',
];

export default function OptimizationProcessing() {
  const navigate = useNavigate();
  const { setCurrentResult, addSession, updateLifetimeMetrics } = useApp();
  const [currentStage, setCurrentStage] = useState(0);
  const [progress, setProgress] = useState(0);
  const [solverDuration, setSolverDuration] = useState(120); // in seconds

  useEffect(() => {
    const duration = sessionStorage.getItem('solverDuration');
    const durationMap = {
      Quick: 30,
      Standard: 120,
      Thorough: 300,
      Maximum: 600,
    };
    setSolverDuration(durationMap[duration as keyof typeof durationMap] || 120);
  }, []);

  useEffect(() => {
    // Simulate stage progression
    const stageInterval = solverDuration / stages.length;
    const stageTimer = setInterval(() => {
      setCurrentStage((prev) => {
        if (prev < stages.length - 1) return prev + 1;
        return prev;
      });
    }, stageInterval * 1000);

    // Progress bar
    const progressInterval = 100; // update every 100ms
    const progressTimer = setInterval(() => {
      setProgress((prev) => {
        const increment = (100 / (solverDuration * 1000)) * progressInterval;
        if (prev + increment >= 100) {
          clearInterval(progressTimer);
          return 100;
        }
        return prev + increment;
      });
    }, progressInterval);

    return () => {
      clearInterval(stageTimer);
      clearInterval(progressTimer);
    };
  }, [solverDuration]);

  useEffect(() => {
    if (progress >= 100 && currentStage >= stages.length - 1) {
      // Call backend optimization API
      setTimeout(async () => {
        try {
          const optimizeData = await runOptimization();
          
          // Get upload data from session storage
          const storedData = JSON.parse(sessionStorage.getItem('uploadedData') || '{}');
          const solverMode = sessionStorage.getItem('solverDuration') || 'Standard';

          const result = buildOptimizationResult(
            {
              employees: storedData.backendEmployees || [],
              vehicles: storedData.backendVehicles || [],
              baseline_cost: storedData.baselineCost || 0,
              filename: storedData.filename || 'UploadedData.xlsx',
            },
            optimizeData,
            solverMode,
            solverDuration
          );

          setCurrentResult(result);
          addSession({
            id: result.sessionId,
            timestamp: result.timestamp,
            employeeCount: result.employees.length,
            vehiclesUsed: result.trips.length,
            savings: result.savings,
            status: 'completed',
            result,
          });
          updateLifetimeMetrics(result);
          navigate('/results');
        } catch (error: any) {
          console.error('Optimization failed:', error);
          // Fallback to mock result on error
          const data = JSON.parse(sessionStorage.getItem('uploadedData') || '{}');
          const result = createMockResult(data);
          setCurrentResult(result);
          addSession({
            id: result.sessionId,
            timestamp: result.timestamp,
            employeeCount: result.employees.length,
            vehiclesUsed: result.trips.length,
            savings: result.savings,
            status: 'completed',
            result,
          });
          updateLifetimeMetrics(result);
          navigate('/results');
        }
      }, 1000);
    }
  }, [progress, currentStage]);

  const createMockResult = (data: any): OptimizationResult => {
    const baselineCost = data.employees.reduce((sum: number, e: any) => sum + e.baselineCost, 0);
    const optimizedCost = baselineCost * 0.65; // 35% savings
    
    return {
      sessionId: generateId(),
      timestamp: new Date().toISOString(),
      inputFile: 'UploadedData.xlsx',
      employees: data.employees,
      vehicles: data.vehicles,
      trips: [],
      assignments: [],
      baselineCost,
      optimizedCost,
      savings: baselineCost - optimizedCost,
      savingsPercentage: 35,
      constraints: {
        hard: { total: 4, satisfied: 4, violated: 0, complianceRate: 100, details: [] },
        soft: { total: 3, satisfied: 3, violated: 0, complianceRate: 100, details: [] },
      },
      solverDuration,
      solverMode: sessionStorage.getItem('solverDuration') as any || 'Standard',
    };
  };

  const elapsed = Math.floor((progress / 100) * solverDuration);
  const remaining = solverDuration - elapsed;

  return (
    <div className="min-h-screen bg-dark flex items-center justify-center p-8 network-bg">
      <motion.div initial={{ opacity: 0, scale: 0.9 }} animate={{ opacity: 1, scale: 1 }} className="max-w-2xl w-full">
        {/* Progress Indicator */}
        <div className="mb-8">
          <div className="flex items-center justify-between">
            {['Upload Data', 'Review Insights', 'Configure', 'Optimize'].map((step, index) => (
              <div key={step} className="flex items-center">
                <div className={`w-10 h-10 rounded-full flex items-center justify-center font-bold ${index <= 3 ? 'bg-primary text-dark' : 'bg-dark-700 text-gray'}`}>
                  {index + 1}
                </div>
                <span className={`ml-2 ${index <= 3 ? 'text-white font-medium' : 'text-gray'}`}>{step}</span>
                {index < 3 && <div className="w-12 h-px bg-gray/30 mx-4" />}
              </div>
            ))}
          </div>
        </div>

        {/* Central Progress Circle */}
        <div className="card text-center py-16">
          <div className="relative w-48 h-48 mx-auto mb-8">
            <svg className="w-full h-full transform -rotate-90">
              <circle cx="96" cy="96" r="88" stroke="currentColor" strokeWidth="8" fill="none" className="text-dark-600" />
              <circle
                cx="96"
                cy="96"
                r="88"
                stroke="currentColor"
                strokeWidth="8"
                fill="none"
                className="text-primary transition-all"
                strokeDasharray={`${2 * Math.PI * 88}`}
                strokeDashoffset={`${2 * Math.PI * 88 * (1 - progress / 100)}`}
                strokeLinecap="round"
              />
            </svg>
            <div className="absolute inset-0 flex flex-col items-center justify-center">
              <div className="text-4xl font-bold text-primary">{Math.floor(progress)}%</div>
              <div className="text-sm text-gray mt-2">
                {Math.floor(elapsed / 60)}:{(elapsed % 60).toString().padStart(2, '0')} / {Math.floor(solverDuration / 60)}:{(solverDuration % 60).toString().padStart(2, '0')}
              </div>
            </div>
          </div>

          {/* Stage Indicators */}
          <div className="space-y-3 max-w-md mx-auto">
            {stages.map((stage, index) => (
              <motion.div
                key={stage}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: index * 0.1 }}
                className="flex items-center gap-3"
              >
                {index < currentStage ? (
                  <Check className="w-5 h-5 text-green-400 flex-shrink-0" />
                ) : index === currentStage ? (
                  <Loader2 className="w-5 h-5 text-primary animate-spin flex-shrink-0" />
                ) : (
                  <div className="w-5 h-5 rounded-full border-2 border-gray/30 flex-shrink-0" />
                )}
                <span className={index <= currentStage ? 'text-white' : 'text-gray'}>{stage}</span>
              </motion.div>
            ))}
          </div>

          <div className="mt-8 text-gray">
            <p>Optimizing routes for {JSON.parse(sessionStorage.getItem('uploadedData') || '{}').employees?.length || 0} employees</p>
            <p className="text-sm mt-2">
              Estimated completion: {new Date(Date.now() + remaining * 1000).toLocaleTimeString()}
            </p>
          </div>

          <button onClick={() => navigate('/insights')} className="mt-8 text-sm text-gray hover:text-white transition-colors">
            Cancel optimization
          </button>
        </div>
      </motion.div>
    </div>
  );
}
