import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import ParticleNetwork from '../components/ParticleNetwork';
import { 
  TrendingUp, Users, MapPin, DollarSign, 
  Upload, BarChart3, Brain, Clock, CheckCircle, XCircle,
  ArrowRight, Info
} from 'lucide-react';
import { formatCurrency, formatNumber, formatDate } from '../utils/helpers';

export default function LandingDashboard() {
  const { sessionHistory, lifetimeMetrics, clearHistory } = useApp();

  const metrics = [
    {
      label: 'Total Optimizations',
      value: formatNumber(lifetimeMetrics.totalOptimizations),
      icon: TrendingUp,
      color: 'text-primary',
    },
    {
      label: 'Cumulative Cost Saved',
      value: formatCurrency(lifetimeMetrics.cumulativeSavings),
      icon: DollarSign,
      color: 'text-primary-bright',
    },
    {
      label: 'Total Employees Transported',
      value: formatNumber(lifetimeMetrics.totalEmployees),
      icon: Users,
      color: 'text-blue-400',
    },
    {
      label: 'Total Kilometers Optimized',
      value: formatNumber(lifetimeMetrics.totalKilometers),
      icon: MapPin,
      color: 'text-purple-400',
    },
  ];

  const quickActions = [
    {
      title: 'New Optimization',
      description: 'Upload fresh data and run optimization',
      icon: Upload,
      to: '/upload',
      color: 'primary',
    },
    {
      title: 'View Last Session',
      description: 'Jump directly to most recent results',
      icon: BarChart3,
      to: '/results',
      color: 'blue',
      disabled: sessionHistory.length === 0,
    },
    {
      title: 'About the Algorithm',
      description: 'Learn about our optimization approach',
      icon: Brain,
      to: '/about',
      color: 'purple',
    },
  ];

  return (
    <div className="min-h-screen">
      {/* Hero Section — full viewport height, floating card with particle network */}
      <section className="h-screen p-3">
        <div className="relative w-full h-full rounded-2xl overflow-hidden bg-dark-800/60 backdrop-blur-xl border border-gray/10 shadow-float-xl">
          {/* Interactive Particle Network background */}
          <ParticleNetwork className="z-10" />

          {/* Gradient overlay for depth */}
          <div className="absolute inset-0 z-[1] bg-gradient-to-b from-dark-800/30 via-transparent to-dark-800/60 pointer-events-none" />

          {/* Content — pointer-events-none so mouse passes through to canvas, re-enable on interactive children */}
          <div className="relative z-[15] flex flex-col items-center justify-center h-full px-8 pointer-events-none">
            <motion.div
              initial={{ opacity: 0, y: 30 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ duration: 0.8, ease: 'easeOut' }}
              className="text-center"
            >
              <motion.h1
                className="text-7xl md:text-8xl font-display font-extrabold mb-4 tracking-tight"
                initial={{ opacity: 0, scale: 0.9 }}
                animate={{ opacity: 1, scale: 1 }}
                transition={{ duration: 1, delay: 0.2 }}
              >
                <span className="text-gradient">VELORA</span>
              </motion.h1>

              <motion.p
                className="text-2xl md:text-3xl text-gray/80 font-light mb-3 tracking-wide"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ duration: 0.8, delay: 0.5 }}
              >
                Driven by Possibility
              </motion.p>

              <motion.p
                className="text-sm text-gray/40 mb-10 max-w-md mx-auto"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ duration: 0.8, delay: 0.7 }}
              >
                Intelligent vehicle routing optimization that transforms logistics complexity into elegant, cost-saving solutions.
              </motion.p>

              <motion.div
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.6, delay: 0.9 }}
              >
                <Link to="/upload" className="pointer-events-auto">
                  <motion.button
                    whileHover={{ scale: 1.05, transition: { duration: 0.1 } }}
                    whileTap={{ scale: 0.95 }}
                    className="bg-primary text-dark text-lg font-bold px-8 py-4 rounded-lg inline-flex items-center gap-3 group hover:bg-primary-light transition-colors duration-100 shadow-float"
                    style={{ opacity: 1 }}
                  >
                    Start Optimization
                    <ArrowRight className="w-5 h-5 font-bold group-hover:translate-x-1 transition-transform" />
                  </motion.button>
                </Link>
              </motion.div>
            </motion.div>
          </div>
        </div>
      </section>

      {/* Metrics + Quick Actions — side by side */}
      <section className="px-3 pb-3">
        <div className="flex flex-col lg:flex-row gap-3" style={{ minHeight: '320px' }}>
          {/* Performance Metrics — 2x2 grid, left side */}
          <div className="lg:w-[55%] bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 shadow-float">
            <h2 className="text-lg font-bold mb-5 text-white/90">Performance Metrics</h2>
            <div className="grid grid-cols-2 gap-4 h-[calc(100%-3rem)]">
              {metrics.map((metric, index) => (
                <motion.div
                  key={metric.label}
                  initial={{ opacity: 0, y: 20 }}
                  whileInView={{ opacity: 1, y: 0 }}
                  viewport={{ once: true }}
                  transition={{ delay: index * 0.15, duration: 0.6 }}
                  className="relative bg-dark-700/50 rounded-xl p-4 flex flex-col border border-gray/5 select-none overflow-hidden shadow-float hover:shadow-float-lg transition-all duration-300"
                  style={{ cursor: 'default' }}
                >
                  {/* Background icon - zoomed, right-aligned, cropped */}
                  <metric.icon className="absolute -right-4 -bottom-4 w-32 h-32 text-primary/10 pointer-events-none" />
                  
                  {/* Content */}
                  <div className="relative z-10 flex flex-col h-full">
                    <p className="text-base text-white/90 font-semibold leading-tight mb-4">{metric.label}</p>
                    <p className="text-3xl font-bold text-primary mt-auto">{metric.value}</p>
                  </div>
                </motion.div>
              ))}
            </div>
          </div>

          {/* Quick Actions — 3 stacked, right side */}
          <div className="lg:w-[45%] bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 flex flex-col shadow-float">
            <h2 className="text-lg font-bold mb-5 text-white/90">Quick Actions</h2>
            <div className="flex flex-col gap-3 flex-1">
              {quickActions.map((action, index) => (
                <Link
                  key={action.title}
                  to={action.to}
                  className={`flex-1 ${action.disabled ? 'pointer-events-none opacity-50' : ''}`}
                >
                  <motion.div
                    initial={{ opacity: 0, y: 20 }}
                    whileInView={{ opacity: 1, y: 0 }}
                    viewport={{ once: true }}
                    transition={{ delay: index * 0.15, duration: 0.6 }}
                    whileHover={{ scale: action.disabled ? 1 : 1.02, transition: { duration: 0.1 } }}
                    className="bg-primary hover:bg-primary-light rounded-xl p-4 h-full flex items-center gap-4 transition-colors cursor-pointer select-none group shadow-float hover:shadow-float-lg duration-100"
                    style={{ cursor: action.disabled ? 'default' : 'pointer', opacity: 1 }}
                  >
                    <div className="w-12 h-12 rounded-lg bg-dark/20 flex items-center justify-center flex-shrink-0">
                      <action.icon className="w-6 h-6 text-dark" />
                    </div>
                    <div className="min-w-0 flex items-center gap-2">
                      <h3 className="text-base font-semibold text-dark">{action.title}</h3>
                      <div className="relative group/info">
                        <Info className="w-4 h-4 text-dark/50 hover:text-dark/80 cursor-help transition-colors" />
                        <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/70 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                          {action.description}
                          <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                        </div>
                      </div>
                    </div>
                    <ArrowRight className="w-5 h-5 text-dark/60 group-hover:text-dark group-hover:translate-x-1 ml-auto flex-shrink-0 transition-all duration-100" />
                  </motion.div>
                </Link>
              ))}
            </div>
          </div>
        </div>
      </section>

      {/* Session History */}
      <section className="px-3 pb-3">
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 shadow-float">
          <div className="flex items-center justify-between mb-6">
            <h2 className="text-lg font-bold text-white/90">Session History</h2>
          {sessionHistory.length > 0 && (
            <button
              onClick={clearHistory}
              className="text-sm text-red-400 hover:text-red-300 transition-colors"
            >
              Clear History
            </button>
          )}
        </div>

        {sessionHistory.length === 0 ? (
          <div className="bg-dark-700/50 rounded-xl border border-gray/5 text-center py-12">
            <Clock className="w-12 h-12 text-gray/40 mx-auto mb-4" />
            <p className="text-gray/50 text-sm">No optimization history yet. Start your first optimization to see results here.</p>
          </div>
        ) : (
          <div className="space-y-3">
            {sessionHistory.map((session, index) => (
              <motion.div
                key={session.id}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: index * 0.05 }}
                className="bg-dark-700/50 rounded-xl p-4 flex items-center justify-between border border-gray/5 shadow-float hover:shadow-float-lg transition-all duration-300"
              >
                <div className="flex items-center gap-4">
                  <div className="flex-shrink-0">
                    {session.status === 'completed' ? (
                      <CheckCircle className="w-8 h-8 text-primary-bright" />
                    ) : (
                      <XCircle className="w-8 h-8 text-red-400" />
                    )}
                  </div>
                  <div className="flex items-center gap-2 text-sm text-gray">
                    <Clock className="w-4 h-4" />
                    {formatDate(session.timestamp)}
                  </div>
                  <div className="h-8 w-px bg-gray/20" />
                  <div className="flex gap-6 text-sm">
                    <div>
                      <span className="text-gray">Employees:</span>{' '}
                      <span className="text-white font-medium">{session.employeeCount}</span>
                    </div>
                    <div>
                      <span className="text-gray">Vehicles:</span>{' '}
                      <span className="text-white font-medium">{session.vehiclesUsed}</span>
                    </div>
                    <div>
                      <span className="text-gray">Savings:</span>{' '}
                      <span className="text-primary font-medium">{formatCurrency(session.savings)}</span>
                    </div>
                  </div>
                </div>
                <div>
                  <span className={`badge ${session.status === 'completed' ? 'badge-success' : 'badge-error'}`}>
                    {session.status}
                  </span>
                </div>
              </motion.div>
            ))}
          </div>
        )}
        </div>
      </section>
    </div>
  );
}

