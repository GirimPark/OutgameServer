IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'[dbo].[Friends]') AND type in (N'U'))
BEGIN
    CREATE TABLE [dbo].[Friends] (
        UserName NVARCHAR(255) COLLATE SQL_Latin1_General_CP1_CS_AS NOT NULL,
        FriendName NVARCHAR(255) COLLATE SQL_Latin1_General_CP1_CS_AS NOT NULL,
        IsMutualFriend BIT NOT NULL DEFAULT 0,
        PRIMARY KEY (UserName, FriendName),
        FOREIGN KEY (UserName) REFERENCES [dbo].[User](Nickname),
        FOREIGN KEY (FriendName) REFERENCES [dbo].[User](Nickname)
    );
END