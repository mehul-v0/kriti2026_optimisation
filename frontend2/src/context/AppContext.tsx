import React, { createContext, useContext, useState, useEffect } from 'react';
import type { ReactNode } from 'react';
import type { OptimizationResult, SessionHistory, LifetimeMetrics } from '../types';

interface AppContextType {
  currentResult: OptimizationResult | null;
  setCurrentResult: (result: OptimizationResult | null) => void;
  sessionHistory: SessionHistory[];
  addSession: (session: SessionHistory) => void;
  clearHistory: () => void;
  lifetimeMetrics: LifetimeMetrics;
  updateLifetimeMetrics: (result: OptimizationResult) => void;
}

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
