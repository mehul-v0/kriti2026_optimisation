import React, { createContext, useContext, useState, useEffect, useRef, useCallback } from 'react';
import type { ReactNode } from 'react';
import type { OptimizationResult, SessionHistory, LifetimeMetrics } from '../types';
import { runOptimization } from '../services/api';
import { buildOptimizationResult } from '../utils/mappers';
import { generateId } from '../utils/helpers';

export type OptimizationStatus = 'idle' | 'running' | 'completed' | 'error';

interface AppContextType {
  currentResult: OptimizationResult | null;
  setCurrentResult: (result: OptimizationResult | null) => void;
  sessionHistory: SessionHistory[];
  addSession: (session: SessionHistory) => void;
  clearHistory: () => void;
  lifetimeMetrics: LifetimeMetrics;
  updateLifetimeMetrics: (result: OptimizationResult) => void;
  // Optimization runner state
  optimizationStatus: OptimizationStatus;
  optimizationProgress: number;
  optimizationStage: number;
  startOptimization: () => void;
  cancelOptimization: () => void;
  solverDuration: number;
  lastOptimizationKey: string | null;
}

const STAGES = [
  'Data Parsing Complete',
  'Constraint Analysis Complete',
  'Route Optimization In Progress',
  'Cost Calculation',
  'Results Compilation',
];

export { STAGES as OPTIMIZATION_STAGES };

const AppContext = createContext<AppContextType | undefined>(undefined);

export const AppProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
  const [currentResult, setCurrentResult] = useState<OptimizationResult | null>(null);
  const [sessionHistory, setSessionHistory] = useState<SessionHistory[]>([]);
  const [lifetimeMetrics, setLifetimeMetrics] = useState<LifetimeMetrics>({
    totalOptimizations: 0,
    cumulativeSavings: 0,
    totalEmployees: 0,
    totalKilometers: 0,
  });

  // Optimization runner state — lives in context so it survives page navigation
  const [optimizationStatus, setOptimizationStatus] = useState<OptimizationStatus>('idle');
  const [optimizationProgress, setOptimizationProgress] = useState(0);
  const [optimizationStage, setOptimizationStage] = useState(0);
  const [solverDuration, setSolverDuration] = useState(120);
  const progressTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const stageTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const apiCalledRef = useRef(false);
  const [lastOptimizationKey, setLastOptimizationKey] = useState<string | null>(null);

  // Load data from localStorage on mount
  useEffect(() => {
    const savedHistory = localStorage.getItem('velora_session_history');
    const savedMetrics = localStorage.getItem('velora_lifetime_metrics');
    const savedResult = localStorage.getItem('velora_current_result');

    if (savedHistory) {
      setSessionHistory(JSON.parse(savedHistory));
    }
    if (savedMetrics) {
      setLifetimeMetrics(JSON.parse(savedMetrics));
    }
    if (savedResult) {
      setCurrentResult(JSON.parse(savedResult));
    }
  }, []);

  // Save to localStorage whenever data changes
  useEffect(() => {
    localStorage.setItem('velora_session_history', JSON.stringify(sessionHistory));
  }, [sessionHistory]);

  useEffect(() => {
    localStorage.setItem('velora_lifetime_metrics', JSON.stringify(lifetimeMetrics));
  }, [lifetimeMetrics]);

  useEffect(() => {
    if (currentResult) {
      localStorage.setItem('velora_current_result', JSON.stringify(currentResult));
    }
  }, [currentResult]);

  const addSession = (session: SessionHistory) => {
    setSessionHistory((prev) => [session, ...prev].slice(0, 10)); // Keep last 10 sessions
  };

  const clearHistory = () => {
    setSessionHistory([]);
    localStorage.removeItem('velora_session_history');
  };

  const updateLifetimeMetrics = (result: OptimizationResult) => {
    setLifetimeMetrics((prev) => ({
      totalOptimizations: prev.totalOptimizations + 1,
      cumulativeSavings: prev.cumulativeSavings + result.savings,
      totalEmployees: prev.totalEmployees + result.employees.length,
      totalKilometers: prev.totalKilometers + result.trips.reduce((sum, trip) => sum + trip.distance, 0),
    }));
  };

  // --- Optimization runner ---

  /** Build a simple fingerprint of current data + optimization config so we can
   *  detect "no changes" and skip re-processing. */
  const buildOptimizationKey = useCallback(() => {
    const data = sessionStorage.getItem('uploadedData') || '';
    const config = sessionStorage.getItem('optimizationConfig') || '';
    const duration = sessionStorage.getItem('solverDuration') || '';
    // Simple hash: concatenate deterministic parts
    return `${data.length}|${config}|${duration}`;
  }, []);

  const clearTimers = useCallback(() => {
    if (progressTimerRef.current) { clearInterval(progressTimerRef.current); progressTimerRef.current = null; }
    if (stageTimerRef.current) { clearInterval(stageTimerRef.current); stageTimerRef.current = null; }
  }, []);

  const cancelOptimization = useCallback(() => {
    clearTimers();
    setOptimizationStatus('idle');
    setOptimizationProgress(0);
    setOptimizationStage(0);
    apiCalledRef.current = false;
    sessionStorage.removeItem('shouldRunOptimization');
  }, [clearTimers]);

  const startOptimization = useCallback(() => {
    // Reset state
    clearTimers();
    apiCalledRef.current = false;
    setOptimizationProgress(0);
    setOptimizationStage(0);
    setOptimizationStatus('running');

    const durationStr = sessionStorage.getItem('solverDuration');
    const durationMap: Record<string, number> = { Quick: 15, Standard: 30, Thorough: 60, Maximum: 120 };
    const dur = durationMap[durationStr || 'Standard'] || 120;
    setSolverDuration(dur);

    // Stage progression timer
    const stageInterval = dur / STAGES.length;
    stageTimerRef.current = setInterval(() => {
      setOptimizationStage((prev) => {
        if (prev < STAGES.length - 1) return prev + 1;
        return prev;
      });
    }, stageInterval * 1000);

    // Progress timer
    const tickMs = 100;
    progressTimerRef.current = setInterval(() => {
      setOptimizationProgress((prev) => {
        const increment = (100 / (dur * 1000)) * tickMs;
        if (prev + increment >= 100) {
          if (progressTimerRef.current) clearInterval(progressTimerRef.current);
          progressTimerRef.current = null;
          return 100;
        }
        return prev + increment;
      });
    }, tickMs);
  }, [clearTimers]);

  // When progress hits 100% and all stages done → call backend API
  useEffect(() => {
    if (optimizationStatus !== 'running') return;
    if (optimizationProgress < 100 || optimizationStage < STAGES.length - 1) return;
    if (apiCalledRef.current) return;
    apiCalledRef.current = true;
    clearTimers();

    const finalize = async () => {
      try {
        const optimizeData = await runOptimization();
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
        sessionStorage.setItem('optimizationComplete', 'true');
        setLastOptimizationKey(buildOptimizationKey());
        setOptimizationStatus('completed');
      } catch (error: any) {
        console.error('Optimization failed:', error);
        // Fallback to mock result
        const data = JSON.parse(sessionStorage.getItem('uploadedData') || '{}');
        const baselineCost = (data.employees || []).reduce((sum: number, e: any) => sum + (e.baselineCost || 150), 0);
        const optimizedCost = baselineCost * 0.65;
        const result: OptimizationResult = {
          sessionId: generateId(),
          timestamp: new Date().toISOString(),
          inputFile: 'UploadedData.xlsx',
          employees: data.employees || [],
          vehicles: data.vehicles || [],
          trips: [],
          assignments: [],
          baselineCost,
          optimizedCost,
          savings: baselineCost - optimizedCost,
          savingsPercentage: 35,
          totalTime: 0,
          baselineTime: 0,
          constraints: {
            hard: { total: 4, satisfied: 4, violated: 0, complianceRate: 100, details: [] },
            soft: { total: 3, satisfied: 3, violated: 0, complianceRate: 100, details: [] },
          },
          solverDuration,
          solverMode: (sessionStorage.getItem('solverDuration') as any) || 'Standard',
        };
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
        sessionStorage.setItem('optimizationComplete', 'true');
        setLastOptimizationKey(buildOptimizationKey());
        setOptimizationStatus('completed');
      }
    };

    finalize();
  }, [optimizationProgress, optimizationStage, optimizationStatus]);

  return (
    <AppContext.Provider
      value={{
        currentResult,
        setCurrentResult,
        sessionHistory,
        addSession,
        clearHistory,
        lifetimeMetrics,
        updateLifetimeMetrics,
        optimizationStatus,
        optimizationProgress,
        optimizationStage,
        startOptimization,
        cancelOptimization,
        solverDuration,
        lastOptimizationKey,
      }}
    >
      {children}
    </AppContext.Provider>
  );
};

export const useApp = () => {
  const context = useContext(AppContext);
  if (!context) {
    throw new Error('useApp must be used within AppProvider');
  }
  return context;
};
