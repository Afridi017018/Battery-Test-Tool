flowchart TD
    Start([Start Battery Health Tool]) --> Init[Initialize Application]
    Init --> CheckOS{Check Operating System}
    
    CheckOS -->|Windows| WinAPI[Use Windows Battery API]
    CheckOS -->|Linux| LinuxAPI[Use /sys/class/power_supply]
    CheckOS -->|Mac| MacAPI[Use IOKit Framework]
    
    WinAPI --> GetData[Retrieve Battery Data]
    LinuxAPI --> GetData
    MacAPI --> GetData
    
    GetData --> ParseData[Parse Battery Information]
    ParseData --> CalcHealth[Calculate Battery Health]
    
    CalcHealth --> CheckCapacity{Design Capacity<br/>Available?}
    CheckCapacity -->|Yes| CalcPercent[Health % = Current/Design × 100]
    CheckCapacity -->|No| EstimateHealth[Estimate from Cycle Count]
    
    CalcPercent --> DisplayInfo[Display Battery Information]
    EstimateHealth --> DisplayInfo
    
    DisplayInfo --> ShowDetails[Show Details:<br/>- Current Capacity<br/>- Design Capacity<br/>- Health Percentage<br/>- Cycle Count<br/>- Charge Status<br/>- Voltage<br/>- Temperature]
    
    ShowDetails --> HealthStatus{Battery Health}
    
    HealthStatus -->|>80%| Good[Status: Good<br/>Color: Green]
    HealthStatus -->|50-80%| Fair[Status: Fair<br/>Color: Yellow]
    HealthStatus -->|<50%| Poor[Status: Poor<br/>Color: Red]
    
    Good --> Recommend[Display Recommendations]
    Fair --> Recommend
    Poor --> Recommend
    
    Recommend --> SaveOption{User Wants to<br/>Save Report?}
    SaveOption -->|Yes| SaveReport[Generate & Save Report<br/>JSON/CSV/PDF]
    SaveOption -->|No| RefreshOption
    
    SaveReport --> RefreshOption{User Wants to<br/>Refresh Data?}
    RefreshOption -->|Yes| GetData
    RefreshOption -->|No| ExitOption
    
    ExitOption{Exit Application?}
    ExitOption -->|Yes| End([End])
    ExitOption -->|No| DisplayInfo
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style Good fill:#90EE90
    style Fair fill:#FFD700
    style Poor fill:#FF6347
    style CalcHealth fill:#87CEEB
    style DisplayInfo fill:#DDA0DD