import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { 
  TrendingUp, Users, MapPin, DollarSign, 
  Upload, BarChart3, Brain, Clock, CheckCircle, XCircle 
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
      color: 'text-green-400',
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
    <div className="min-h-screen bg-dark">
      {/* Hero Section */}
      <section className="relative overflow-hidden px-8 py-16  bg-gradient-dark network-bg">
        <div className="relative z-10 max-w-6xl mx-auto">
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.6 }}
            className="text-center"
          >
            <h1 className="text-6xl font-bold mb-4 font-display">
              <span className="text-gradient">VELORA</span>
            </h1>
            <p className="text-2xl text-gray mb-8">Driven by Possibility</p>
            <Link to="/upload">
              <motion.button
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
                className="btn-primary text-lg px-8 py-4"
              >
                Start New Optimization
              </motion.button>
            </Link>
          </motion.div>
        </div>

        {/* Animated background elements */}
        <div className="absolute inset-0 overflow-hidden pointer-events-none">
          <div className="absolute top-20 left-10 w-64 h-64 bg-primary/5 rounded-full blur-3xl animate-pulse-slow" />
          <div className="absolute bottom-20 right-10 w-96 h-96 bg-primary/5 rounded-full blur-3xl animate-pulse-slow" style={{ animationDelay: '1s' }} />
        </div>
      </section>

      {/* Key Metrics */}
      <section className="px-8 py-12 max-w-7xl mx-auto">
        <h2 className="text-2xl font-bold mb-6">Performance Metrics</h2>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
          {metrics.map((metric, index) => (
            <motion.div
              key={metric.label}
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: index * 0.1 }}
              className="card"
            >
              <div className="flex items-start justify-between">
                <div>
                  <p className="text-sm text-gray mb-2">{metric.label}</p>
                  <p className={`text-3xl font-bold ${metric.color}`}>{metric.value}</p>
                </div>
                <metric.icon className={`w-8 h-8 ${metric.color}`} />
              </div>
            </motion.div>
          ))}
        </div>
      </section>

      {/* Quick Actions */}
      <section className="px-8 py-12 max-w-7xl mx-auto">
        <h2 className="text-2xl font-bold mb-6">Quick Actions</h2>
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
          {quickActions.map((action, index) => (
            <Link
              key={action.title}
              to={action.to}
              className={action.disabled ? 'pointer-events-none opacity-50' : ''}
            >
              <motion.div
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: index * 0.1 }}
                whileHover={{ scale: action.disabled ? 1 : 1.02 }}
                className="card-hover h-full"
              >
                <action.icon className={`w-12 h-12 mb-4 text-${action.color}-400`} />
                <h3 className="text-xl font-bold mb-2">{action.title}</h3>
                <p className="text-gray text-sm">{action.description}</p>
              </motion.div>
            </Link>
          ))}
        </div>
      </section>

      {/* Session History */}
      <section className="px-8 py-12 max-w-7xl mx-auto pb-20">
        <div className="flex items-center justify-between mb-6">
          <h2 className="text-2xl font-bold">Session History</h2>
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
          <div className="card text-center py-12">
            <Clock className="w-16 h-16 text-gray mx-auto mb-4" />
            <p className="text-gray">No optimization history yet. Start your first optimization to see results here.</p>
          </div>
        ) : (
          <div className="space-y-4">
            {sessionHistory.map((session, index) => (
              <motion.div
                key={session.id}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: index * 0.05 }}
                className="card-hover flex items-center justify-between"
              >
                <div className="flex items-center gap-4">
                  <div className="flex-shrink-0">
                    {session.status === 'completed' ? (
                      <CheckCircle className="w-8 h-8 text-green-400" />
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
      </section>
    </div>
  );
}
