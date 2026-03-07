import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import ParticleNetwork from '../components/ParticleNetwork';
import { formatCurrency, formatNumber, formatDate } from '../utils/helpers';

export default function LandingDashboard() {
  const { sessionHistory, lifetimeMetrics, clearHistory } = useApp();

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
      iconColor: 'text-primary',
      change: '+8%',
      glow: true,
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
    <div className="max-w-7xl mx-auto p-6 md:p-8 space-y-8">
      {/* Hero Section */}
      <div className="relative w-full rounded-xl overflow-hidden glass-card min-h-[320px] flex items-center justify-center p-8 border border-border-dark">
        {/* Particle Network background */}
        <ParticleNetwork className="z-0" />
        <div className="absolute inset-0 z-[1] bg-gradient-to-r from-background-dark/90 to-background-dark/70 pointer-events-none" />

        <div className="relative z-10 flex flex-col items-center text-center max-w-2xl gap-6">
          <div className="inline-flex items-center justify-center px-3 py-1 rounded-full bg-primary/20 text-primary border border-primary/30 text-xs font-medium uppercase tracking-wider mb-2">
            AI-Powered Logistics
          </div>
          <h1 className="text-4xl md:text-5xl font-black text-slate-900 dark:text-white tracking-tight leading-tight">
            Vehicle Routing Optimization
          </h1>
          <p className="text-base md:text-lg text-slate-600 dark:text-slate-300 max-w-xl">
            Optimize your corporate logistics and fleet operations efficiently. Reduce costs, minimize carbon footprint, and streamline delivery routes.
          </p>
          <Link to="/upload">
            <motion.button
              whileHover={{ scale: 1.04 }}
              whileTap={{ scale: 0.97 }}
              className="mt-4 px-8 py-3.5 bg-primary text-background-dark font-bold rounded-lg shadow-lg hover:bg-primary/90 transition-all flex items-center gap-2"
            >
              <span className="material-symbols-outlined">rocket_launch</span>
              Start New Optimization
            </motion.button>
          </Link>
        </div>
      </div>

      {/* KPI Cards */}
      <div>
        <h2 className="text-xl font-bold mb-4 text-slate-900 dark:text-white flex items-center gap-2">
          <span className="material-symbols-outlined text-primary">monitoring</span>
          Lifetime Performance
        </h2>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          {metrics.map((metric, index) => (
            <motion.div
              key={metric.label}
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: index * 0.1, duration: 0.5 }}
              className={`glass-card rounded-xl p-5 flex flex-col gap-2 ${metric.glow ? 'relative overflow-hidden' : ''}`}
            >
              {metric.glow && (
                <div className="absolute top-0 right-0 w-24 h-24 bg-primary/10 rounded-full blur-xl -mr-10 -mt-10 pointer-events-none" />
              )}
              <div className="flex justify-between items-start">
                <p className="text-sm font-medium text-slate-500 dark:text-slate-400">{metric.label}</p>
                <span className={`material-symbols-outlined text-sm ${metric.iconColor || 'text-slate-400'}`}>{metric.icon}</span>
              </div>
              <p className="text-3xl font-bold text-slate-900 dark:text-white mt-1">{metric.value}</p>
              <div className="flex items-center gap-1 mt-auto">
                <span className="text-primary text-xs font-semibold bg-primary/10 px-1.5 py-0.5 rounded flex items-center">
                  <span className="material-symbols-outlined text-[12px]">arrow_upward</span> {metric.change}
                </span>
                <span className="text-xs text-slate-500">vs last month</span>
              </div>
            </motion.div>
          ))}
        </div>
      </div>

      {/* Session History & Quick Actions */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Session History Table */}
        <div className="lg:col-span-2 glass-card rounded-xl border border-border-dark overflow-hidden flex flex-col">
          <div className="p-5 border-b border-border-dark flex justify-between items-center bg-[rgba(255,255,255,0.025)]">
            <h2 className="text-lg font-bold text-slate-900 dark:text-white flex items-center gap-2">
              <span className="material-symbols-outlined text-slate-400">history</span>
              Session History
            </h2>
            {sessionHistory.length > 0 && (
              <button className="text-xs font-medium text-primary hover:text-primary/80 transition-colors">View All</button>
            )}
          </div>
          <div className="overflow-x-auto">
            {sessionHistory.length === 0 ? (
              <div className="text-center py-16 px-6">
                <span className="material-symbols-outlined text-slate-500/40 text-5xl mb-4 block">history</span>
                <p className="text-slate-500 dark:text-slate-400 text-sm">No optimization history yet. Start your first optimization to see results here.</p>
              </div>
            ) : (
              <table className="w-full text-left border-collapse">
                <thead>
                  <tr className="border-b border-border-dark bg-[rgba(255,255,255,0.015)]">
                    <th className="px-5 py-3 text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">Date</th>
                    <th className="px-5 py-3 text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">Cost Saved</th>
                    <th className="px-5 py-3 text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">Employees</th>
                    <th className="px-5 py-3 text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">Outcome</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-border-dark">
                  {sessionHistory.map((session) => (
                    <tr key={session.id} className="hover:bg-surface-dark-hover transition-colors">
                      <td className="px-5 py-4 text-sm text-slate-700 dark:text-slate-300 font-medium">{formatDate(session.timestamp)}</td>
                      <td className="px-5 py-4 text-sm text-slate-600 dark:text-slate-400">{formatCurrency(session.savings)}</td>
                      <td className="px-5 py-4 text-sm text-slate-600 dark:text-slate-400">{session.employeeCount}</td>
                      <td className="px-5 py-4">
                        {session.status === 'completed' ? (
                          <span className="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium bg-primary/10 text-primary border border-primary/20">
                            <span className="w-1.5 h-1.5 rounded-full bg-primary"></span> Success
                          </span>
                        ) : (
                          <span className="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium bg-red-500/10 text-red-400 border border-red-500/20">
                            <span className="w-1.5 h-1.5 rounded-full bg-red-400"></span> Failed
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
        <div className="glass-card rounded-xl border border-border-dark p-5 flex flex-col h-full">
          <h2 className="text-lg font-bold text-slate-900 dark:text-white mb-4 flex items-center gap-2">
            <span className="material-symbols-outlined text-slate-400">bolt</span>
            Quick Actions
          </h2>
          <div className="flex flex-col gap-3 flex-1">
            <Link to="/results">
              <button className="w-full flex items-center justify-between px-4 py-3 bg-surface-dark hover:bg-surface-dark-hover border border-border-dark rounded-lg transition-colors group">
                <div className="flex items-center gap-3">
                  <span className="material-symbols-outlined text-primary">visibility</span>
                  <span className="text-sm font-medium">View Last Session</span>
                </div>
                <span className="material-symbols-outlined text-slate-500 group-hover:text-slate-300 text-sm transition-colors">arrow_forward</span>
              </button>
            </Link>
            <button className="w-full flex items-center justify-between px-4 py-3 bg-surface-dark hover:bg-surface-dark-hover border border-border-dark rounded-lg transition-colors group">
              <div className="flex items-center gap-3">
                <span className="material-symbols-outlined text-slate-400">psychology</span>
                <span className="text-sm font-medium">About Algorithm</span>
              </div>
              <span className="material-symbols-outlined text-slate-500 group-hover:text-slate-300 text-sm transition-colors">arrow_forward</span>
            </button>
            {sessionHistory.length > 0 && (
              <button
                onClick={clearHistory}
                className="w-full flex items-center justify-between px-4 py-3 bg-surface-dark hover:bg-surface-dark-hover border border-border-dark rounded-lg transition-colors group mt-auto"
              >
                <div className="flex items-center gap-3">
                  <span className="material-symbols-outlined text-slate-400">delete_sweep</span>
                  <span className="text-sm font-medium">Clear History</span>
                </div>
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

