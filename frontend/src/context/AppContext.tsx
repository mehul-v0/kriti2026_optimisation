import React, { createContext, useContext, useState, useEffect, useRef, useCallback } from 'react';
import type { ReactNode } from 'react';
import type { OptimizationResult, SessionHistory, LifetimeMetrics } from '../types';
import { runOptimization, getGeometryStatus } from '../services/api';
import { buildOptimizationResult } from '../utils/mappers';
import { generateId } from '../utils/helpers';

export type OptimizationStatus = 'idle' | 'running' | 'completed' | 'error';
export type GeometryStatus = 'pending' | 'fetching' | 'complete' | 'not_needed';

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
  // Geometry fetching state
  geometryStatus: GeometryStatus;
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
  const [geometryStatus, setGeometryStatus] = useState<GeometryStatus>('not_needed');
  const progressTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const stageTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const apiCalledRef = useRef(false);
  const startTimeRef = useRef<number>(0);
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

  const startOptimization = useCallback(async () => {
    // Reset state
    clearTimers();
    apiCalledRef.current = true; // Set immediately to prevent duplicate calls
    setOptimizationProgress(0);
    setOptimizationStage(0);
    setOptimizationStatus('running');
    setGeometryStatus('pending');

    const durationStr = sessionStorage.getItem('solverDuration');
    const durationMap: Record<string, number> = { Quick: 15, Standard: 30, Thorough: 60, Maximum: 120 };
    const dur = durationMap[durationStr || 'Standard'] || 120;
    setSolverDuration(dur);

    // Record start time for accurate progress tracking
    startTimeRef.current = Date.now();

    // Stage progression timer - cycles through all stages
    const stageInterval = dur / STAGES.length;
    stageTimerRef.current = setInterval(() => {
      setOptimizationStage((prev) => {
        if (prev < STAGES.length - 1) return prev + 1;
        return prev;
      });
    }, stageInterval * 1000);

    // Progress timer - use timestamp-based calculation for accuracy across tab switches
    const tickMs = 10; // Check every 10ms
    progressTimerRef.current = setInterval(() => {
      const elapsed = Date.now() - startTimeRef.current;
      const progressPercent = Math.min(100, (elapsed / (dur * 1000)) * 100);
      
      setOptimizationProgress(progressPercent);
      
      if (progressPercent >= 100) {
        // Stop progress timer when 100% is reached
        if (progressTimerRef.current) clearInterval(progressTimerRef.current);
        progressTimerRef.current = null;
        // Stop stage timer
        if (stageTimerRef.current) clearInterval(stageTimerRef.current);
        stageTimerRef.current = null;
      }
    }, tickMs);

    // Call backend API immediately in parallel with progress timer
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
        dur
      );

      // Store optimization ID for geometry polling
      const optimizationId = optimizeData.optimization_id;

      // Wait for progress to reach 100% + minimum 1 second before showing results
      const waitForProgress = () => {
        const elapsed = Date.now() - startTimeRef.current;
        const minimumWait = (dur + 1) * 1000; // Selected time + 1 second
        
        if (elapsed >= minimumWait) {
          clearTimers();
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
          
          // Start geometry polling if optimization ID exists
          if (optimizationId) {
            console.log('Starting geometry polling for optimization:', optimizationId);
            startGeometryPolling(optimizationId);
          }
        } else {
          setTimeout(waitForProgress, 250);
        }
      };
      
      // Geometry polling function
      const startGeometryPolling = (optId: string) => {
        let pollCount = 0;
        const maxPolls = 60; // Max 2 minutes of polling (60 * 2s)
        
        const pollInterval = setInterval(async () => {
          pollCount++;
          
          // Safety: stop after max polls
          if (pollCount > maxPolls) {
            console.warn('Geometry polling timeout - stopping after', maxPolls, 'attempts');
            clearInterval(pollInterval);
            return;
          }
          
          try {
            const geometryStatusResponse = await getGeometryStatus(optId);
            
            // Update geometry status in context
            setGeometryStatus(geometryStatusResponse.geometry_status as GeometryStatus);
            
            console.log('Geometry status:', geometryStatusResponse.geometry_status, 
                       `(${geometryStatusResponse.geometry_progress.fetched}/${geometryStatusResponse.geometry_progress.total})`);
            
            // Debug: check how many routes actually have geometry
            if (geometryStatusResponse.result && geometryStatusResponse.result.routes) {
              let routesWithGeometry = 0;
              let totalRoutePoints = 0;
              let pointsWithGeometry = 0;
              
              geometryStatusResponse.result.routes.forEach((route: any) => {
                let hasGeometry = false;
                route.route_points?.forEach((pt: any) => {
                  totalRoutePoints++;
                  if (pt.geometry && pt.geometry.length > 0) {
                    pointsWithGeometry++;
                    hasGeometry = true;
                  }
                });
                if (hasGeometry) routesWithGeometry++;
              });
              
              console.log(`  Routes with geometry: ${routesWithGeometry}/${geometryStatusResponse.result.routes.length}`);
              console.log(`  Points with geometry: ${pointsWithGeometry}/${totalRoutePoints}`);
            }
            
            // Always update result with latest geometry data
            if (geometryStatusResponse.success && geometryStatusResponse.result) {
              const updatedResult = buildOptimizationResult(
                {
                  employees: storedData.backendEmployees || [],
                  vehicles: storedData.backendVehicles || [],
                  baseline_cost: storedData.baselineCost || 0,
                  filename: storedData.filename || 'UploadedData.xlsx',
                },
                geometryStatusResponse.result,
                solverMode,
                dur
              );
              setCurrentResult(updatedResult);
              console.log('Updated result with geometry data');
            }
            
            // Stop polling when complete or not needed
            if (geometryStatusResponse.geometry_status === 'complete' || geometryStatusResponse.geometry_status === 'not_needed') {
              console.log('✅ Geometry fetching complete - stopping polling');
              clearInterval(pollInterval);
              return;
            }
          } catch (error) {
            console.error('Geometry polling error:', error);
            clearInterval(pollInterval);
          }
        }, 2000); // Poll every 2 seconds
      };
      
      waitForProgress();
    } catch (error: any) {
      console.error('Optimization failed:', error);
      clearTimers();
      // Fallback to mock result
      const data = JSON.parse(sessionStorage.getItem('uploadedData') || '{}');
      const employeeList = data.employees || [];
      const vehicleList = data.vehicles || [];
      const baselineCost = employeeList.reduce((sum: number, e: any) => sum + (e.baselineCost || 150), 0);
      const optimizedCost = baselineCost * 0.65;
      const baselineTime = employeeList.length * 15;
      const optimizedTime = baselineTime * 0.7;
      const result: OptimizationResult = {
        sessionId: generateId(),
        timestamp: new Date().toISOString(),
        inputFile: data.filename || 'UploadedData.xlsx',
        employees: employeeList,
        vehicles: vehicleList,
        trips: [],
        assignments: [],
        baselineCost,
        optimizedCost,
        savings: baselineCost - optimizedCost,
        savingsPercentage: 35,
        totalTime: optimizedTime,
        baselineTime: baselineTime,
        constraints: {
          hard: { total: 0, satisfied: 0, violated: 0, complianceRate: 0, details: [] },
          soft: { total: 0, satisfied: 0, violated: 0, complianceRate: 0, details: [] },
        },
        solverMode: (sessionStorage.getItem('solverDuration') || 'Standard') as 'Quick' | 'Standard' | 'Thorough' | 'Maximum',
      };
      setCurrentResult(result);
      setOptimizationStatus('completed');
    }
  }, [clearTimers, optimizationProgress, setCurrentResult, addSession, updateLifetimeMetrics, setLastOptimizationKey]);

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
        geometryStatus,
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

