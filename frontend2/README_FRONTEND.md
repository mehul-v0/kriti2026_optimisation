# VELORA - Vehicle Route Optimization Frontend

A professional, aesthetically appealing React frontend for the Velora vehicle routing optimization system.

## 🎨 Design Features

- **Dark theme** with lime accent color (#D9EF92)
- **Professional UI** inspired by modern logistics/delivery platforms
- **Smooth animations** using Framer Motion
- **Responsive design** for all screen sizes
- **11 comprehensive pages** covering the entire optimization workflow

## 🚀 Quick Start

### Installation

```bash
cd frontend2
npm install
```

### Development

```bash
npm run dev
```

The application will start on `http://localhost:3001`

### Build for Production

```bash
npm run build
```

## 📁 Project Structure

```
frontend2/
├── src/
│   ├── components/         # Reusable components
│   │   ├── Layout.tsx      # Main layout wrapper
│   │   └── Sidebar.tsx     # Navigation sidebar
│   ├── context/
│   │   └── AppContext.tsx  # Global state management
│   ├── pages/              # Page components (11 pages)
│   │   ├── LandingDashboard.tsx
│   │   ├── DataUpload.tsx
│   │   ├── DataInsights.tsx
│   │   ├── OptimizationProcessing.tsx
│   │   ├── ResultsOverview.tsx
│   │   ├── ConstraintValidation.tsx
│   │   ├── RouteExplorer.tsx
│   │   ├── VehicleFleet.tsx
│   │   ├── EmployeeAssignments.tsx
│   │   ├── CostBreakdown.tsx
│   │   └── ExportReports.tsx
│   ├── types/
│   │   └── index.ts        # TypeScript type definitions
│   ├── utils/
│   │   └── helpers.ts      # Utility functions
│   ├── App.tsx             # Main app with routing
│   ├── main.tsx            # Application entry point
│   └── index.css           # Global styles with Tailwind
├── public/                 # Static assets (place images here)
├── package.json
├── tailwind.config.js
└── vite.config.ts
```

## 🎯 Key Features

### 1. Landing Dashboard
- Lifetime metrics display (stored in localStorage)
- Session history timeline
- Quick action cards
- Animated hero section

### 2. Data Upload
- Drag-and-drop file upload
- Excel file parsing (.xlsx, .xls)
- Real-time validation
- Google Drive & Dropbox integration (UI ready)

### 3. Data Insights Preview
- Time window visualization
- Employee and vehicle statistics
- Priority and fuel type distributions
- Solver configuration (Quick/Standard/Thorough/Maximum)

### 4. Optimization Processing
- Real-time progress indicator
- Stage-by-stage progress
- Estimated completion time
- Simulated optimization (can connect to backend API)

### 5. Results Overview
- Primary savings summary
- Key performance metrics
- Quick navigation to detailed views

### 6. Constraint Validation
- Hard vs Soft constraints comparison
- Compliance rate visualization
- Detailed constraint breakdown

### 7. Route Explorer
- Map visualization placeholder (ready for Leaflet/Mapbox integration)
- Vehicle route display
- Interactive controls

### 8. Vehicle Fleet Analysis
- Fleet composition breakdown
- Vehicle utilization metrics
- Fuel type distribution

### 9. Employee Assignments
- Searchable employee table
- Priority-based filtering
- Assignment status tracking

### 10. Cost Breakdown
- Baseline vs optimized cost comparison
- Per-employee cost impact
- Projection calculator (daily/monthly/annual)

### 11. Export & Reports
- Multiple export formats (JSON, Excel, PDF)
- Custom report builder
- Data persistence management

## 🔌 Backend Integration

The frontend expects a backend API at `/api/optimize`. To connect to your Python backend:

1. Update `vite.config.ts` proxy settings if your backend runs on a different port
2. The optimization processing page sends POST requests to `/api/optimize`
3. Expected request format: JSON with employees and vehicles arrays
4. Expected response format: OptimizationResult type (see src/types/index.ts)

## 🎨 Color Palette

- **Primary**: #D9EF92 (Lime, used for accents and CTAs)
- **Dark**: #000000 (Main background)
- **Dark variants**: #1a1a1a, #2a2a2a, #3a3a3a
- **Gray**: #808284 (Text and borders)
- **White**: #FFFFFF (Primary text)

## 📦 Dependencies

- **React 19** - UI framework
- **React Router DOM** - Navigation
- **Framer Motion** - Animations
- **Tailwind CSS** - Styling
- **Lucide React** - Icons
- **XLSX** - Excel file parsing
- **Recharts** - Charts (ready to use)
- **Leaflet** - Maps (ready to use)
- **jsPDF** - PDF generation (ready to use)

## 💾 Data Persistence

The application uses browser localStorage for:
- Session history (last 10 optimizations)
- Lifetime metrics (cumulative statistics)
- Current optimization results

Data persists across browser sessions but is specific to each browser/device.

## 🔧 Customization

### Adding New Pages
1. Create component in `src/pages/`
2. Add route in `src/App.tsx`
3. Add navigation item in `src/components/Sidebar.tsx`

### Styling
- Tailwind utility classes are used throughout
- Custom components defined in `src/index.css`
- Animation keyframes in `src/index.css`

### Theme Colors
Update colors in `tailwind.config.js` to customize the color scheme.

## 📱 Responsive Design

The application is fully responsive:
- Mobile: Single column layouts, collapsible sidebars
- Tablet: 2-column grids
- Desktop: Multi-column layouts with full sidebar

## ⚡ Performance

- Code splitting by route
- Lazy loading for large components
- Optimized animations
- Efficient re-renders with React context

## 🐛 Known Limitations

1. **Map Integration**: Route Explorer has a placeholder - integrate Leaflet/Mapbox for full functionality
2. **Export Features**: Excel and PDF exports need full implementation with xlsx and jsPDF libraries
3. **Google Drive/Dropbox**: UI is ready but needs OAuth integration
4. **Backend Connection**: Currently uses mock data - connect to actual optimization API

## 📄 License

Copyright © 2026 Velora Optimization
