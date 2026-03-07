import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useCallback, useRef, useState } from 'react';
import { useApp } from '../context/AppContext';
import { formatCurrency, formatNumber, formatDate } from '../utils/helpers';

export default function LandingDashboard() {
  const { sessionHistory, lifetimeMetrics, clearHistory } = useApp();

  // Mouse spotlight for grid
  const heroRef = useRef<HTMLDivElement>(null);
  const [mousePos, setMousePos] = useState<{ x: number; y: number } | null>(null);
  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    const rect = heroRef.current?.getBoundingClientRect();
    if (rect) setMousePos({ x: e.clientX - rect.left, y: e.clientY - rect.top });
  }, []);
  const handleMouseLeave = useCallback(() => setMousePos(null), []);

  const metrics = [
    {
      label: 'Total Optimizations',
      value: formatNumber(lifetimeMetrics.totalOptimizations),
      icon: 'route',
      change: '+15%',
    },
    {
      label: 'Cumulative Cost Saved',
      value: formatCurrency(lifetimeMetrics.cumulativeSavings),
      icon: 'savings',
      change: '+8%',
    },
    {
      label: 'Employees Transported',
      value: formatNumber(lifetimeMetrics.totalEmployees),
      icon: 'groups',
      change: '+5%',
    },
    {
      label: 'Total KM Optimized',
      value: formatNumber(lifetimeMetrics.totalKilometers),
      icon: 'directions_car',
      change: '+12%',
    },
  ];

  return (
    <div className="max-w-[1400px] mx-auto p-6 md:p-8 space-y-8">
      {/* Hero Section */}
      <div
        ref={heroRef}
        onMouseMove={handleMouseMove}
        onMouseLeave={handleMouseLeave}
        className="relative w-full bg-panel-dark border border-white/10 border-t-2 border-t-primary overflow-hidden min-h-[420px] flex items-center justify-center p-8"
      >
        {/* Grid Background */}
        <div className="absolute inset-0 pointer-events-none" style={{
          backgroundImage: `
            linear-gradient(rgba(255,184,0,0.04) 1px, transparent 1px),
            linear-gradient(90deg, rgba(255,184,0,0.04) 1px, transparent 1px)
          `,
          backgroundSize: '48px 48px',
        }} />
        {/* Mouse spotlight - brighter grid near cursor */}
        {mousePos && (
          <div className="absolute inset-0 pointer-events-none transition-opacity duration-200" style={{
            backgroundImage: `
              linear-gradient(rgba(255,184,0,0.15) 1px, transparent 1px),
              linear-gradient(90deg, rgba(255,184,0,0.15) 1px, transparent 1px)
            `,
            backgroundSize: '48px 48px',
            maskImage: `radial-gradient(circle 180px at ${mousePos.x}px ${mousePos.y}px, black, transparent)`,
            WebkitMaskImage: `radial-gradient(circle 180px at ${mousePos.x}px ${mousePos.y}px, black, transparent)`,
          }} />
        )}
        {/* Radial fade so grid fades at edges */}
        <div className="absolute inset-0 pointer-events-none" style={{
          background: 'radial-gradient(ellipse at center, transparent 30%, #0D1117 80%)',
        }} />
        <div className="relative z-10 flex flex-col items-center text-center max-w-2xl gap-5">
          <span className="px-2 py-0.5 bg-white/5 border border-white/10 text-[9px] font-mono text-white/50 uppercase tracking-widest">
            AI-Powered Logistics // v2.0
          </span>
          <h1 className="text-4xl md:text-5xl font-black text-white tracking-tight uppercase leading-tight">
            Vehicle Routing<br />Optimization
          </h1>
          <p className="text-sm text-white/40 font-mono max-w-xl leading-relaxed">
            Optimize your corporate logistics and fleet operations efficiently.
            Reduce costs, minimize carbon footprint, and streamline delivery routes.
          </p>
          <Link to="/upload">
            <motion.button
              whileHover={{ scale: 1.03 }}
              whileTap={{ scale: 0.97 }}
              className="mt-4 bg-primary text-background-dark font-label font-bold py-3 px-8 text-sm tracking-widest uppercase glow-amber transition-all flex items-center gap-2"
            >
              <span className="material-symbols-outlined text-[18px]">rocket_launch</span>
              Start Optimization
            </motion.button>
          </Link>
        </div>
      </div>

      {/* KPI Cards */}
      <div>
        <div className="flex items-center gap-3 mb-4">
          <span className="material-symbols-outlined text-primary text-[18px]">monitoring</span>
          <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">
            Lifetime Performance
          </h2>
        </div>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          {metrics.map((metric, index) => (
            <motion.div
              key={metric.label}
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: index * 0.1, duration: 0.5 }}
              className="bg-panel-dark border border-white/10 p-5 flex flex-col gap-2"
            >
              <div className="flex justify-between items-start">
                <p className="text-[11px] font-label uppercase tracking-widest text-white/40">{metric.label}</p>
                <span className="material-symbols-outlined text-sm text-white/20">{metric.icon}</span>
              </div>
              <p className="text-3xl font-bold font-mono text-white mt-1">{metric.value}</p>
              <div className="flex items-center gap-2 mt-auto">
                <span className="text-[9px] font-mono text-primary bg-primary/10 px-1.5 py-0.5 flex items-center gap-0.5">
                  <span className="material-symbols-outlined text-[11px]">arrow_upward</span>{metric.change}
                </span>
                <span className="text-[10px] font-mono text-white/30">vs last month</span>
              </div>
            </motion.div>
          ))}
        </div>
      </div>

      {/* Session History & Quick Actions */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Session History Table */}
        <div className="lg:col-span-2 bg-panel-dark border border-white/10 overflow-hidden flex flex-col">
          <div className="p-5 border-b border-white/10 flex justify-between items-center bg-white/[0.02]">
            <div className="flex items-center gap-3">
              <span className="material-symbols-outlined text-white/20 text-[18px]">history</span>
              <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">
                Session History
              </h2>
            </div>
            {sessionHistory.length > 0 && (
              <button className="text-[10px] font-mono text-primary hover:text-primary/80 transition-colors uppercase tracking-wider">
                View All
              </button>
            )}
          </div>
          <div className="overflow-x-auto">
            {sessionHistory.length === 0 ? (
              <div className="text-center py-16 px-6">
                <span className="material-symbols-outlined text-white/10 text-5xl mb-4 block">history</span>
                <p className="text-xs font-mono text-white/30">
                  No optimization history yet. Start your first optimization to see results here.
                </p>
              </div>
            ) : (
              <table className="w-full text-left border-collapse">
                <thead>
                  <tr className="border-b border-white/10 bg-white/[0.015]">
                    <th className="px-5 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Date</th>
                    <th className="px-5 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Cost Saved</th>
                    <th className="px-5 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Employees</th>
                    <th className="px-5 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Outcome</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-white/5">
                  {sessionHistory.map((session) => (
                    <tr key={session.id} className="hover:bg-white/[0.03] transition-colors">
                      <td className="px-5 py-4 text-sm font-mono text-white/70">{formatDate(session.timestamp)}</td>
                      <td className="px-5 py-4 text-sm font-mono text-white/50">{formatCurrency(session.savings)}</td>
                      <td className="px-5 py-4 text-sm font-mono text-white/50">{session.employeeCount}</td>
                      <td className="px-5 py-4">
                        {session.status === 'completed' ? (
                          <span className="inline-flex items-center gap-1.5 px-2 py-0.5 bg-primary/10 text-primary border border-primary/20 text-[9px] font-mono uppercase tracking-wider">
                            <span className="w-1.5 h-1.5 bg-primary"></span> Success
                          </span>
                        ) : (
                          <span className="inline-flex items-center gap-1.5 px-2 py-0.5 bg-action/10 text-action border border-action/20 text-[9px] font-mono uppercase tracking-wider">
                            <span className="w-1.5 h-1.5 bg-action"></span> Failed
                          </span>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </div>

        {/* Quick Actions */}
        <div className="bg-panel-dark border border-white/10 p-5 flex flex-col h-full">
          <div className="flex items-center gap-3 mb-4">
            <span className="material-symbols-outlined text-white/20 text-[18px]">bolt</span>
            <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">
              Quick Actions
            </h2>
          </div>
          <div className="flex flex-col gap-3 flex-1">
            <Link to="/results">
              <button className="w-full flex items-center justify-between px-4 py-3 border border-white/10 bg-white/[0.02] hover:bg-white/5 transition-colors group">
                <div className="flex items-center gap-3">
                  <span className="material-symbols-outlined text-primary text-[18px]">visibility</span>
                  <span className="text-xs font-label font-bold uppercase tracking-wider text-white/60">View Last Session</span>
                </div>
                <span className="material-symbols-outlined text-white/20 group-hover:text-white/40 text-sm transition-colors">arrow_forward</span>
              </button>
            </Link>
            <button className="w-full flex items-center justify-between px-4 py-3 border border-white/10 bg-white/[0.02] hover:bg-white/5 transition-colors group">
              <div className="flex items-center gap-3">
                <span className="material-symbols-outlined text-white/30 text-[18px]">psychology</span>
                <span className="text-xs font-label font-bold uppercase tracking-wider text-white/60">About Algorithm</span>
              </div>
              <span className="material-symbols-outlined text-white/20 group-hover:text-white/40 text-sm transition-colors">arrow_forward</span>
            </button>
            {sessionHistory.length > 0 && (
              <button
                onClick={clearHistory}
                className="w-full flex items-center justify-between px-4 py-3 border border-white/10 bg-white/[0.02] hover:bg-action/10 transition-colors group mt-auto"
              >
                <div className="flex items-center gap-3">
                  <span className="material-symbols-outlined text-action/60 text-[18px]">delete_sweep</span>
                  <span className="text-xs font-label font-bold uppercase tracking-wider text-white/60">Clear History</span>
                </div>
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

